#define _USE_MATH_DEFINES
#include <cmath>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <iostream>
#include <glm.hpp>
#include <unordered_map>
#include <vector>
#include <bitset>
#include <algorithm>
#include <gtx/string_cast.hpp>
#ifndef CXXOPTS_NO_EXCEPTIONS
#define CXXOPTS_NO_EXCEPTIONS
#endif
#include "cxxopts.hpp"
#include "ioHelper.h"


// From CMAKE preprocessor
std::string cmakelists_dir = CMAKELISTS_SOURCE_DIR;


class Options {
public:
	// all Option members are set through the command line args

	std::string inputPath;             // path to folder that contains the light field dataset
	std::string inputJsonPath;         // path to the .json with the input light field camera parameters
	std::string outputPath;       // output mesh file (.bin)

	std::vector<InputCamera> inputCameras;

	unsigned int SCR_WIDTH = 1920;    // width in pixels of the ImGUI window
	unsigned int SCR_HEIGHT = 1080;   // height in pixels of the ImGUI window

	int nrFrames = 0;               // nr video frames
	int outputNrFrames = 1;
	int StartingFrameNr = 0;        // the number of the video frame that will be shown first
	
	bool headless = true;
	bool verbose = false;

	// some tunable shader uniforms:
	float triangle_deletion_margin = 100.0f;        // used in geometry shader for the threshold for stretched triangle deletion
public:

	Options(){}

	Options(int argc, char* argv[]) {

		cxxopts::Options options("CreateMeshes", "Convert a dataset of color and depth videos to one simplified mesh per frame.");
		options.add_options()
			("h,help", "Print help")
			;
		options.add_options("Input videos/images")
			("i,input_dir", "Path to the folder that contains the light field images/videos", cxxopts::value<std::string>())
			("j,input_json", "Path to the .json file with the input light field camera parameters", cxxopts::value<std::string>())
			;
		options.add_options("Saving to disk")
			// save to disk
			("o,output_bin", "File (.bin) where the output mesh will be saved", cxxopts::value<std::string>())
			;
		options.add_options("Settings to improve quality")
			("triangle_deletion_margin", "The higher this value, the less strict the threshold for deletion of stretched triangles.", cxxopts::value<float>()->default_value("100.0"))
			;
		options.add_options("Misc.")
			("gui", "Disable headless mode to display a GUI with the masks and edge maps.")
			("v,verbose", "Verbose prints")
			;

		cxxopts::ParseResult result = options.parse(argc, argv);
		// print help if necessary
		if (argc < 2 || result.count("help"))
		{
			std::cout << options.help({ "Input videos/images" , "Saving to disk", "Settings to improve quality", "Misc."}) << std::endl;
			exit(0);
		}
		// filter out common errors in the user - provided files and paths
		if (!inputAndOutputFilesOK(result)) {
			exit(-1);
		}

		if (result.count("triangle_deletion_margin")) {
			triangle_deletion_margin = result["triangle_deletion_margin"].as<float>();
			if (triangle_deletion_margin < 1) {
				std::cout << "Option --triangle_deletion_margin should be at least 1" << std::endl;
				exit(-1);
			}
		}
		if (result.count("gui")) {
			headless = false;
		}
		if (result.count("verbose")) {
			verbose = true;
		}
	}
private:
	bool dirExists(const std::string path){
		struct stat info;

		if (stat(path.c_str(), &info) != 0)
			return false;
		else if (info.st_mode & S_IFDIR)
			return true;
		else
			return false;
	}

	bool fileExists(const std::string path) {
		struct stat buffer;
		return (stat(path.c_str(), &buffer) == 0);
	}

	std::string getFolderFromFile(const std::string file) {
		size_t strpos = file.find_last_of("/\\");
		if (strpos == std::string::npos) {
			return ".";
		}
		return file.substr(0, strpos + 1);
	}

	// filter out common errors in the user-provided files and paths
	bool inputAndOutputFilesOK(cxxopts::ParseResult result) {
		// input_json and input_dir are required
		if (result.count("input_json"))
		{
			inputJsonPath = result["input_json"].as<std::string>();
		}
		else {
			std::cout << "Missing required argument -j or --input_json" << std::endl;
			return false;
		}
		if (result.count("input_dir"))
		{
			inputPath = result["input_dir"].as<std::string>();
		}
		else {
			std::cout << "Missing required argument -i or --input_dir" << std::endl;
			return false;
		}

		// some optional output options
		if (result.count("output_bin"))
		{
			outputPath = result["output_bin"].as<std::string>();
		}
		// check if inputJsonPath and outputJsonPath are existing files
		if (!fileExists(inputJsonPath)) {
			std::cout << "Error: could not open file " << inputJsonPath << std::endl;
			return false;
		}
		// check if inputPath and outputPath and the folder that contains fpsCsvPath are existing folders
		if (!dirExists(inputPath)) {
			std::cout << "Error: could not find folder " << inputPath << std::endl;
			return false;
		}
		if (outputPath == "") {
			std::cout << "Error: missing argument -o " << std::endl;
			return false;
		}

		// make sure inputPath ends with a \\ or /
		size_t strpos = inputPath.find_last_of("/\\");
		if (strpos != inputPath.size()-1) {
			inputPath = inputPath + "/";
		}
		// read in inputJsonPath
		if (!readInputJson(inputJsonPath, inputPath, /*out*/ inputCameras, nrFrames)) {
			return false;
		}
		if (inputCameras.size() == 0) {
			std::cout << "Error: the JSON did not contain any input cameras" << std::endl;
			return false;
		}
		if (inputCameras[0].res_x % 4 != 0 || inputCameras[0].res_y % 4 != 0) {
			std::cout << "Error: the resolution of the cameras should be a multiple of 4 along both dimensions (for OpenGL)" << std::endl;
			return false;
		}

		if (inputCameras[0].res_x < 1 || inputCameras[0].res_x > 8192 || inputCameras[0].res_y < 1 || inputCameras[0].res_y > 8192) {
			std::cout << "Error: input image/video width and height need to be within [1, 8192] pixels." << std::endl;
			return false;
		}

		// check if .mp4 files are provided, and if all inputs have the same type
		std::string inputFileType = "mp4";
		for (InputCamera input : inputCameras) {
			std::string fileTypes[2] = { input.pathColor.substr(input.pathColor.size() - 3, 3), input.pathDepth.substr(input.pathDepth.size() - 3, 3) };
			for (std::string fileType : fileTypes) {
				if (fileType != inputFileType) {
					std::cout << "Error: all input cameras in the JSON need to have the same file type, i.e. the names need to end with .mp4" << std::endl;
					return false;
				}
			}
		}

		// check if all inputs and outputs have the same resolution and projection (and hor_range, ver_range, fov if relevant)
		int input_width = inputCameras[0].res_x;
		int input_height = inputCameras[0].res_y;
		Projection input_proj = inputCameras[0].projection;
		glm::vec2 input_hor_range = inputCameras[0].hor_range;
		glm::vec2 input_ver_range = inputCameras[0].ver_range;
		float input_fov = inputCameras[0].fov;
		for (InputCamera input : inputCameras) {
			if (input.res_x != input_width || input.res_y != input_height) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same resolution" << std::endl;
				return false;
			}
			if (input.projection != input_proj) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same Projection" << std::endl;
				return false;
			}
			if (input_proj == Projection::Equirectangular && (input.hor_range != input_hor_range || input.ver_range != input_ver_range)) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same Hor_range and Ver_range" << std::endl;
				return false;
			}
			if (input_proj == Projection::Fisheye_equidistant && (input.fov != input_fov)) {
				std::cout << "Error: ALL input cameras in the JSON file need to have the same Fov" << std::endl;
				return false;
			}
		}

		return true;
	}
};


#include "AppDecUtils.h"
#include "Application.h"

int main(int argc, char* argv[]){


	Options options = Options(argc, argv);

	Application app(options, options.inputCameras);
	if (!app.Init())
	{
		app.Shutdown();
		return 1;
	}

	app.RunMeshLoop(options.outputPath);
	
	app.Shutdown();
	

	return 0;
}
