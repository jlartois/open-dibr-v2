#ifndef APPLICATION_H
#define APPLICATION_H

#include <cuda.h>
#include <cudaGL.h> // CUDA OpenGL interop needed for cuGraphicsGLRegisterImage

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "FFmpegDemuxer.h"
#include "shader.h"
#include "ioHelper.h"
#include "ShaderController.h"
#include "TexController.h"
#include "FramebufferController.h"
#include "Simplify.h"

void flipVertically(std::vector<unsigned char>& img, int width, int height)
{
	std::vector<unsigned char> tmp(width);
	for (int y = 0; y < height / 2; ++y) {
		unsigned char* rowTop = img.data() + y * width;
		unsigned char* rowBottom = img.data() + (height - 1 - y) * width;
		std::memcpy(tmp.data(), rowTop, width);
		std::memcpy(rowTop, rowBottom, width);
		std::memcpy(rowBottom, tmp.data(), width);
	}
}

inline uint8_t popcount(uint8_t x) {
	uint8_t c = 0;
	while (x) {
		x &= (x - 1);
		++c;
	}
	return c;
}


class Application
{
protected:
	Options options;
	std::vector<InputCamera> inputCameras;
	ShaderController shaders;
	FrameBufferController framebuffers;
	TexController textures;

	GLFWwindow* window = NULL;

	int screenWidth = 0;
	int screenHeight = 0;

	// ImGui
	int guiCamIdx = 0;
	enum MaskType
	{
		MASK = 0,
		DILATED = 1,
		EDGE = 2
	};
	int guiMaskType = MaskType::MASK;

public:
	Application(Options options, std::vector<InputCamera> inputCameras)
		: options(options)
		, inputCameras(inputCameras) {};

	bool Init()
	{
		// chose the window resolution so it fits the dispay, and has the same aspect ratio as the input images/videos
		float scale = std::min(float(options.SCR_WIDTH) / inputCameras[0].res_x, float(options.SCR_HEIGHT) / inputCameras[0].res_y);
		screenWidth = int(inputCameras[0].res_x * scale);
		screenHeight = int(inputCameras[0].res_y * scale);

		if ((screenWidth % 4) != 0) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			printf("Changed GL_UNPACK_ALIGNMENT from 4 to 1\n");
		}

		glfwInit();
		if(options.headless) glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
		window = glfwCreateWindow(screenWidth, screenHeight, "CreateMeshes", NULL, NULL);
		glfwMakeContextCurrent(window);
		glfwSwapInterval(0); // disable vsync

		if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
			std::cerr << "Failed to initialize GLAD\n";
			return false;
		}

		int xpos, ypos;
		glfwGetMonitorPos(glfwGetPrimaryMonitor(), &xpos, &ypos);
		glfwSetWindowPos(window, xpos, ypos + 20); // Move window to top-left

		glEnable(GL_DEPTH_TEST);

		if (!shaders.Init(inputCameras[0], options)) return false;
		if (!textures.Init(inputCameras, options.verbose)) return false;
		if (!framebuffers.Init(inputCameras, options)) return false;

		return true;
	}

	void Shutdown()
	{
		framebuffers.cleanup();
		textures.Cleanup();

		if (window != NULL) glfwDestroyWindow(window);
		glfwTerminate();
	}

	void ProcessInput(bool& quit) {
		// check if we should quit
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) quit = true;
	}

	void RunMainLoop()
	{
		// prepare OpenGL and ImGui for the render loop
		glViewport(0, 0, screenWidth, screenHeight);
		ImGui::CreateContext();
		ImGui_ImplGlfw_InitForOpenGL(window, true);
		ImGui_ImplOpenGL3_Init("#version 330");

		// render loop
		printf("Start GUI loop\n");
		bool quit = false;
		while (!glfwWindowShouldClose(window) && !quit) {

			ProcessInput(quit);
			RenderImGUI();

			// copy texture to screen
			shaders.showMaskShader.use();
			switch (guiMaskType) {
			case MaskType::MASK:
				framebuffers.ShowMask(textures.images[guiCamIdx], textures.masks[guiCamIdx]);
				break;
			case MaskType::DILATED:
				framebuffers.ShowMask(textures.images[guiCamIdx], textures.masks_dilated[guiCamIdx]);
				break;
			case MaskType::EDGE:
				framebuffers.ShowMask(textures.images[guiCamIdx], textures.edges[guiCamIdx]);
				break;
			}

			// finalize
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
			glfwPollEvents();
		}

		// cleanup ImGui
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void RenderImGUI() {
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		ImGui::Begin("Options");
		ImGui::SliderInt("Image Idx", &guiCamIdx, 0, inputCameras.size() - 1);

		ImGui::Text("Texture to show:");
		ImGui::RadioButton("Mask (before erosion and dilation)", &guiMaskType, MaskType::MASK);
		ImGui::RadioButton("Mask (after erosion and dilation)", &guiMaskType, MaskType::DILATED);
		ImGui::RadioButton("Edge map", &guiMaskType, MaskType::EDGE);
		ImGui::End();
	}

	void RunMeshLoop(std::string outPath) {
		for (int frame = 0; frame < options.nrFrames; frame++) { 
			if (!textures.DecodeNextVideoFrame()) return;
			CalculateMasksAndEdgeMaps();

			if(!options.headless) RunMainLoop();

			WriteSimplifiedMeshesToFile(outPath, frame==0, options.verbose);
			printf("frame %d / %d done\n", frame + 1, options.nrFrames);
		}
	}
	
	void DownloadDecodedVideoFrame(int i, /*out*/ std::vector<unsigned short>& z) {
		// download depth
		glBindTexture(GL_TEXTURE_2D, textures.depths[i]);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_SHORT, z.data());
	}
	
	void CalculateMasksAndEdgeMaps() {
		glViewport(0, 0, inputCameras[0].res_x, inputCameras[0].res_y);
		shaders.shader.use();
		int nrViews = inputCameras.size();
		for (int iter = 0; iter < nrViews - 1; iter++) {
			int mainIdx = iter;
			shaders.updateInputParams(inputCameras[mainIdx]);
			for (int neighborIdx = mainIdx + 1; neighborIdx < nrViews; neighborIdx++) {
				shaders.updateOutputParams(inputCameras[neighborIdx]);
				framebuffers.WarpToNeighbor(
					textures.depths[mainIdx],
					textures.depths[neighborIdx],
					textures.masks[neighborIdx], // mask = 0 means unique pixel, mask = 1 means pixel already covered by other view
					iter == 0
				);
			}
		}

		// erode masks
		shaders.erodeShader.use();
		for (int i = 0; i < nrViews; i++) {
			framebuffers.ProcessUInt8Tex(textures.masks[i], textures.masks_eroded[i]);
		}
		// dilate masks
		shaders.dilateShader.use();
		for (int i = 0; i < nrViews; i++) {
			framebuffers.ProcessUInt8Tex(textures.masks_eroded[i], textures.masks_dilated[i]);
		}

		// also calculate which parts of the depth map are smooth and which are not (indicating detailed geometry)
		shaders.edgeShader.use();
		for (int i = 0; i < nrViews; i++) {
			framebuffers.CalculateEdgeMask(textures.depths[i], textures.masks_dilated[i], textures.edges[i]);
		}
	}

	void WriteSimplifiedMeshesToFile(std::string outPath, bool isFirstFrame, bool verbose=true) {

		int width = inputCameras[0].res_x;
		int height = inputCameras[0].res_y;
		Simplify::Vertex v;
		Simplify::Triangle t;
		t.keep = false;
		t.deleted = false;
		t.attr = 0;
		t.material = -1;
		std::vector<float> all_vertices;
		std::vector<uint32_t> all_triangles;
		float depth_thresh = 500; // TODO user-defined threshold

		std::ofstream outFile(outPath, isFirstFrame ? std::ios::binary:  std::ios::binary | std::ios_base::app);

		int triangles_offset = 0;
		for (int i = 0; i < inputCameras.size(); i++) {
			InputCamera input = inputCameras[i];
			float near = input.z_near;
			float far = input.z_far;
			float fx = input.focal_x;
			float fy = input.focal_y;
			float cx = input.principal_point_x;
			float cy = input.principal_point_y;
			glm::mat4 model = input.model;

			// download mask (mask == 0 means keep pixel) and edge map (0 means pixel on depth edge)
			int nrPixels = width * height;
			std::vector<unsigned char> mask(nrPixels);
			std::vector<unsigned char> is_edge(nrPixels);
			glBindTexture(GL_TEXTURE_2D, textures.masks_dilated[i]);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, mask.data());
			flipVertically(mask, width, height);
			glBindTexture(GL_TEXTURE_2D, textures.edges[i]);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, is_edge.data());
			flipVertically(is_edge, width, height);

			// also depth
			std::vector<unsigned short> depth(width * height);
			DownloadDecodedVideoFrame(i, depth);

			// turn into mesh
			Simplify::vertices.clear();
			Simplify::triangles.clear();

			if (verbose) printf("\nstart simplifying mesh\n");

			// check for elongated triangles in each square
			std::vector<bool> triangle_is_not_elongated((height-1) * (width-1) * 4, true); // 4 triangles per square
			for (int row = 0; row < height-1; row++) {
				for (int col = 0; col < width-1; col++) {
					float depth_00 = depth[row * width + col];
					float depth_01 = depth[row * width + col + 1];
					float depth_10 = depth[(row + 1) * width + col];
					float depth_11 = depth[(row + 1) * width + col + 1];
			
					bool edge_00_01 = abs(depth_00 - depth_01) < depth_thresh;
					bool edge_00_10 = abs(depth_00 - depth_10) < depth_thresh;
					bool edge_00_11 = abs(depth_00 - depth_11) < depth_thresh;
					bool edge_01_11 = abs(depth_01 - depth_11) < depth_thresh;
					bool edge_10_11 = abs(depth_10 - depth_11) < depth_thresh;
					bool edge_10_01 = abs(depth_10 - depth_01) < depth_thresh;
			
					int offset = (row * (width - 1) + col) * 4;
					triangle_is_not_elongated[offset    ] = edge_00_01 && edge_00_10 && edge_10_01; // top left triangle
					triangle_is_not_elongated[offset + 1] = edge_00_01 && edge_01_11 && edge_00_11; // top right triangle
					triangle_is_not_elongated[offset + 2] = edge_00_10 && edge_10_11 && edge_00_11; // bottom left triangle
					triangle_is_not_elongated[offset + 3] = edge_01_11 && edge_10_11 && edge_10_01; // bottom right triangle
				}
			}

			std::vector<uint8_t> square_types(height * width, 0); // each uint8 contains 4 bools, indicating which vertices are not masked off

			// define vertices
			for (int row = 0; row < height; row++) {
				for (int col = 0; col < width; col++) {
					int pixel = row * width + col;
					v.p.z = -1.0 / (1.0f / far + (depth[pixel] / 65535.0) * (1.0f / near - 1.0f / far));
					v.p.x = -(col + 0.5f - cx) / fx * v.p.z;
					v.p.y = (row + 0.5f - cy) / fy * v.p.z;
					Simplify::vertices.push_back(v);
					
					if (mask[pixel] == 0) {
						// remember that this vert is not masked away (by setting a bit for each square the vert belongs to)
						square_types[row * width + col] += 1;
						if(col > 0) square_types[row * width + col - 1] += 2;
						if (row > 0) square_types[(row - 1) * width + col] += 4;
						if (row > 0 && col > 0) square_types[(row - 1) * width+ col - 1] += 8;
					}
				}
			}
			// define triangles
			int o = 0;
			for (int row = 0; row < height - 1; row++) {
				for (int col = 0; col < width - 1; col++) {

					// check which of the 4 verts were not masked away
					int square_type = square_types[row * width + col];

					// we can only draw triangles if there are 3+ verts
					int nr_valid_verts = popcount(square_type);
					if (nr_valid_verts < 3) {
						o++;
						continue;
					}

					// don't simplify triangles if on edge
					t.keep = (is_edge[row * width + col] > 0) && (mask[row * width + col] == 0);

					int square = 4 * (row * (width-1) + col);
					bool check_every_triangle = false;
					if (square_type == 15) { // two triangles
						// check if all 4 verts have more or less the same depth
						if (triangle_is_not_elongated[square] && triangle_is_not_elongated[square + 2]) {
							// top left triangle
							t.v[0] = o;
							t.v[1] = o + width;
							t.v[2] = o + 1;
							Simplify::triangles.push_back(t);
							// bottom right triangle
							t.v[0] = o + 1;
							t.v[1] = o + width;
							t.v[2] = o + width + 1;
							Simplify::triangles.push_back(t);
						}
						else {
							check_every_triangle = true;
						}
					}
					else if ((check_every_triangle || square_type == 7) && triangle_is_not_elongated[square]) { // top left triangle
						t.v[0] = o;
						t.v[1] = o + width;
						t.v[2] = o + 1;
						Simplify::triangles.push_back(t);
					}
					else if ((check_every_triangle || square_type == 11) && triangle_is_not_elongated[square + 1]) { // top right triangle
						t.v[0] = o;
						t.v[1] = o + width + 1;
						t.v[2] = o + 1;
						Simplify::triangles.push_back(t);
					}
					else if ((check_every_triangle || square_type == 13) && triangle_is_not_elongated[square + 2]) { // bottom left triangle
						t.v[0] = o;
						t.v[1] = o + width;
						t.v[2] = o + width + 1;
						Simplify::triangles.push_back(t);
					}
					else if ((check_every_triangle || square_type == 14) && triangle_is_not_elongated[square + 3]) { // bottom right triangle
						t.v[0] = o + 1;
						t.v[1] = o + width;
						t.v[2] = o + width + 1;
						Simplify::triangles.push_back(t);
					}
					o++;
				}
				o++;
			}

			// simplify mesh (only where edge map says there are no edges)
			if (verbose) printf("-> %d triangles\n", (int)Simplify::triangles.size());
			int target_triangle_count = Simplify::triangles.size() / 70;
			Simplify::simplify_mesh(target_triangle_count, 7.0, true);
			
			// simplify mesh (only where edge map says there are edges)
			if (verbose) printf("-> %d triangles\n", (int)Simplify::triangles.size());
			target_triangle_count = Simplify::triangles.size() / 4;
			Simplify::simplify_mesh(target_triangle_count, 7.0, false);

			std::vector<float> vertices(Simplify::vertices.size() * 3);
			std::vector<uint32_t> triangles(Simplify::triangles.size() * 3);
			for (int j = 0; j < Simplify::vertices.size(); j ++) {
				vertices[3 * j] = Simplify::vertices[j].p.x;
				vertices[3 * j + 1] = Simplify::vertices[j].p.y;
				vertices[3 * j + 2] = Simplify::vertices[j].p.z;
			}
			for (int j = 0; j < Simplify::triangles.size(); j++) {
				triangles[3 * j] = Simplify::triangles[j].v[0] + triangles_offset;
				triangles[3 * j + 1] = Simplify::triangles[j].v[1] + triangles_offset;
				triangles[3 * j + 2] = Simplify::triangles[j].v[2] + triangles_offset;
			}
			if (verbose) printf("end simplification with %d triangles\n", Simplify::triangles.size());


			// to world space
			for (int j = 0; j < vertices.size(); j += 3) {
				glm::vec4 vpos(vertices[j + 0], vertices[j + 1], vertices[j + 2], 1.0f);
				glm::vec4 wpos = model * vpos;
				vertices[j + 0] = wpos.x;
				vertices[j + 1] = wpos.y;
				vertices[j + 2] = wpos.z;
			}

			all_vertices.insert(all_vertices.end(), vertices.begin(), vertices.end());
			all_triangles.insert(all_triangles.end(), triangles.begin(), triangles.end());

			triangles_offset = all_vertices.size() / 3;
		}

		int nr_vertices = all_vertices.size() / 3;
		int nr_triangles = all_triangles.size() / 3;

		// write nr_vertices and nr_triangles
		outFile.write(reinterpret_cast<const char*>(&nr_vertices), sizeof(nr_vertices));
		outFile.write(reinterpret_cast<const char*>(&nr_triangles), sizeof(nr_triangles));
		// write vertex positions, vertex colors and triangle indices
		outFile.write(reinterpret_cast<const char*>(all_vertices.data()), all_vertices.size() * sizeof(float));
		outFile.write(reinterpret_cast<const char*>(all_triangles.data()), all_triangles.size() * sizeof(uint32_t));

		outFile.close();
		if (verbose) printf("Wrote %d vertices, %d triangles to %s\n", nr_vertices, nr_triangles, outPath.c_str());
	}
};


#endif APPLICATION_H
