#ifndef SHADER_CONTROLLER_H
#define SHADER_CONTROLLER_H


class ShaderController {
public:
	Shader shader;         // shader for 3D warping 
	Shader showMaskShader; // copy a uint8 texture to screen
	Shader dilateShader;   // dilate a mask
	Shader erodeShader;    // erode a mask
	Shader edgeShader;     // determine where the edges in a depth map are


public:
	ShaderController() {
		shader = Shader();
		showMaskShader = Shader();
		dilateShader = Shader();
		erodeShader = Shader();
		edgeShader = Shader();
	}

	bool Init(InputCamera input, Options options) {

		std::string basePath = cmakelists_dir + "/src/shaders/";
		std::cout << "Reading GLSL files from " << basePath << std::endl;
		
		if (!shader.init(
			(basePath + "vertex.fs").c_str(),
			(basePath + "fragment.fs").c_str(),
			(basePath + "geometry.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "vertex.fs"
				<< " or " << basePath + "fragment.fs"
				<< " or " << basePath + "geometry.fs" << std::endl;
			return false;
		}
		shader.use();

		float max_error_x = 1.0f / (1.0f / input.z_far + 0.5f / (std::pow(2, input.bitdepth_depth) - 1.0f) * (1.0f / input.z_near - 1.0f / input.z_far));
		float max_error = std::abs(1.0f / (1.0f / input.z_far) - max_error_x);
		float triangle_deletion_factor = max_error / std::pow(max_error_x - input.z_near, 2);
		shader.setFloat("triangle_deletion_factor", triangle_deletion_factor);
		shader.setFloat("triangle_deletion_margin", options.triangle_deletion_margin);
		shader.setFloat("width", float(input.res_x));
		shader.setFloat("height", float(input.res_y));
		shader.setFloat("projection_type", input.projection == Projection::Perspective ? 0.0f : (input.projection == Projection::Equirectangular ? 0.5f : 1.0f));
		if (input.projection == Projection::Equirectangular) {
			shader.setVec2("hor_range", input.hor_range);
			shader.setVec2("ver_range", input.ver_range);
		}
		else if (input.projection == Projection::Fisheye_equidistant) {
			shader.setFloat("fov", input.fov);
		}
		shader.setVec2("near_far", glm::vec2(input.z_near, input.z_far));
		shader.setInt("mainDepthTex", 0);
		shader.setInt("neighborDepthTex", 1);

		if (!showMaskShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "mask_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "mask_fragment.fs" << std::endl;
			return false;
		}
		showMaskShader.use();
		showMaskShader.setInt("imageTex", 0);
		showMaskShader.setInt("maskTex", 1);
		showMaskShader.setFloat("height", float(input.res_y));
		int luma_height = input.res_y;
		int luma_height_rounded = ((luma_height + 16 - 1) / 16) * 16; //round luma height up to multiple of 16
		int texture_height = luma_height_rounded + /*chroma height */luma_height / 2;
		float chroma_offset = float(luma_height_rounded - luma_height);
		showMaskShader.setFloat("chroma_offset", chroma_offset); // TODO

		if (!dilateShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "dilate_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "dilate_fragment.fs" << std::endl;
			return false;
		}
		dilateShader.use();
		dilateShader.setInt("inputTex", 0);
		dilateShader.setFloat("width", float(input.res_x));
		dilateShader.setFloat("height", float(input.res_y));

		if (!erodeShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "erode_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "erode_fragment.fs" << std::endl;
			return false;
		}
		erodeShader.use();
		erodeShader.setInt("inputTex", 0);
		erodeShader.setFloat("width", float(input.res_x));
		erodeShader.setFloat("height", float(input.res_y));

		if (!edgeShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "edge_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "edge_fragment.fs" << std::endl;
			return false;
		}
		edgeShader.use();
		edgeShader.setInt("depthTex", 0);
		edgeShader.setInt("erodedMaskTex", 1);
		edgeShader.setFloat("width", float(input.res_x));
		edgeShader.setFloat("height", float(input.res_y));

		return true;
	}

	void updateInputParams(InputCamera input) {
		shader.use();
		shader.setVec2("near_far", glm::vec2(input.z_near, input.z_far));
		shader.setMat4("model", input.model);
		shader.setVec3("inputCameraPos", input.pos);
		if (input.projection == Projection::Perspective) {
			shader.setVec2("in_f", glm::vec2(input.focal_x, input.focal_y)); 
			shader.setVec2("in_pp", glm::vec2(input.principal_point_x, input.principal_point_y));
		}
	}

	void updateOutputParams(InputCamera output) {
		shader.use();
		shader.setVec2("out_f", glm::vec2(output.focal_x, output.focal_y));
		shader.setVec2("out_near_far", glm::vec2(output.z_near, output.z_far));
		shader.setVec2("out_pp", glm::vec2(output.principal_point_x, output.principal_point_y));
		shader.setMat4("view", output.view);
		shader.setVec3("outputCameraPos", glm::vec3(output.model[3]));
	}
};


#endif
