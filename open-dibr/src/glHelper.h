#ifndef GL_HELPER_H
#define GL_HELPER_H

#include "ioHelper.h"
#include "shader.h"

/*
* The ShaderController initializes the OpenGL shaders (in init()) and
* allows to set uniforms with updateInputParams()
*/
class ShaderController {
public:
	Shader shader;
	Shader copyShader; // shader for simply copying textures between FBOs
	Shader companionWindowShader;  // shader for simply copying textures from a FBO to the screen
	Shader cameraVisibilityShader; // shader to illustrate the positions of the cameras in a separate window
	Shader toInputShader; // to warp the mesh to one of the input cameras and output the depth
	Shader dilateShader;  // to dilate a depth map (make foreground objects bigger)
	Shader inpaintShader;  // to inpaint the holes in the final image


public:
	ShaderController() {}

	bool init(std::vector<InputCamera> inputCameras, Options options, int out_width, int out_height, float chroma_offset, OutputCamera output, std::unordered_set<int> inputsToUse) {
		InputCamera input = inputCameras[0];

		std::string basePath = cmakelists_dir + "/src/";
		std::cout << "Reading GLSL files from " << basePath << std::endl;
		
		if (options.showCameraVisibilityWindow && !cameraVisibilityShader.init(
			(basePath + "cameras_vertex.fs").c_str(),
			(basePath + "cameras_frag.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "cameras_vertex.fs"
				<< " or " << basePath + "cameras_vertex.fs" << std::endl;
			return false;
		}
		if (!shader.init(
			(basePath + "vertex.fs").c_str(),
			(basePath + "fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "vertex.fs"
				<< " or " << basePath + "fragment.fs" << std::endl;
			return false;
		}
		shader.use();
		shader.setFloat("out_width", (float)out_width);
		shader.setFloat("out_height", (float)out_height);
		shader.setFloat("chroma_offset", chroma_offset);
		shader.setInt("nrTextures", inputsToUse.size()); 

		for (int i = 0; i < 4; i++) {
			shader.setInt("colorTex[" + std::to_string(i) + "]", 2 * i);
			shader.setInt("depthTex[" + std::to_string(i) + "]", 2 * i + 1);
		}
		updateInputParams(inputCameras, inputsToUse);

		shader.setFloat("convertYCbCrToRGB", options.saveOutputImages ? 0.0f : 1.0f);
		shader.setFloat("isYCbCr", options.usePNGs? 0.0f : 1.0f);
		shader.setFloat("width", float(input.res_x));
		shader.setFloat("height", float(input.res_y));
		shader.setFloat("isVR", output.isVR? 1.0f : 0.0f);

		if (!copyShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "copy_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "copy_fragment.fs" << std::endl;
			return false;
		}
		copyShader.use();
		copyShader.setInt("previousFBOColorTex", 2);
		copyShader.setInt("previousFBOAngleAndDepthTex", 3);

		if (!companionWindowShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "copy_fragment_1output.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "copy_fragment_1output.fs" << std::endl;
			return false;
		}
		companionWindowShader.use();
		companionWindowShader.setInt("previousFBOColorTex", 0);

		if (!toInputShader.init(
			(basePath + "toinput_vertex.fs").c_str(),
			(basePath + "toinput_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "toinput_vertex.fs"
				<< " or " << basePath + "toinput_fragment.fs" << std::endl;
			return false;
		}
		toInputShader.use();
		toInputShader.setFloat("out_width", (float)input.res_x);
		toInputShader.setFloat("out_height", (float)input.res_y);

		if (!dilateShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "dilate_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "dilate_fragment.fs" << std::endl;
			return false;
		}
		dilateShader.use();
		dilateShader.setFloat("width", (float)input.res_x);
		dilateShader.setFloat("height", (float)input.res_y);
		dilateShader.setInt("inputTex", 0);

		if (!inpaintShader.init(
			(basePath + "copy_vertex.fs").c_str(),
			(basePath + "inpaint_fragment.fs").c_str())) {
			std::cout << "failed to compile " << basePath + "copy_vertex.fs"
				<< " or " << basePath + "inpaint_fragment.fs" << std::endl;
			return false;
		}
		inpaintShader.use();
		inpaintShader.setFloat("width", (float)input.res_x);
		inpaintShader.setFloat("height", (float)input.res_y);
		inpaintShader.setInt("colorTex", 0);
		inpaintShader.setInt("depthTex", 1);

		return true;
	}

	void updateInputParams(std::vector<InputCamera> inputCameras, std::unordered_set<int> inputsToUse) {
		shader.use();
		int i = 0;
		for (auto& idx : inputsToUse) {
			InputCamera input = inputCameras[idx];
			shader.setVec3("inputCameraPos[" + std::to_string(i) + "]", input.pos);
			shader.setMat4("in_view[" + std::to_string(i) + "]", input.view);
			if (input.projection == Projection::Perspective) {
				shader.setVec2("in_f[" + std::to_string(i) + "]", glm::vec2(input.focal_x, input.focal_y));
				shader.setVec2("in_pp[" + std::to_string(i) + "]", glm::vec2(input.principal_point_x, input.principal_point_y));
			}
			i++;
		}
	}

	void updateOutputParams(OutputCamera outputCamera) {
		shader.use();
		if (!outputCamera.isVR) {
			shader.setVec2("out_f", glm::vec2(outputCamera.focal_x, outputCamera.focal_y)); 
			shader.setVec2("out_near_far", glm::vec2(outputCamera.z_near, outputCamera.z_far));
			shader.setVec2("out_pp", glm::vec2(outputCamera.principal_point_x, outputCamera.principal_point_y));
		}
		shader.setMat4("out_view", outputCamera.view);
		shader.setVec3("outputCameraPos", glm::vec3(outputCamera.model[3])); 
	}
};

struct Mesh {
	std::vector<float> vertices;
	std::vector<uint32_t> triangles;
};


class FrameBufferController {
private:
	// FBOs:
	static const int nrFramebuffersPerEye = 3;
	int nrFramebuffers = 6;
	GLuint framebuffers[nrFramebuffersPerEye * 2];
	GLuint outputTexColors[nrFramebuffersPerEye * 2];
	GLuint outputTexAngleAndDepth[nrFramebuffersPerEye * 2];
	GLuint depthrenderbuffers[nrFramebuffersPerEye * 2];
	int index[2] = { 0 , 0 }; // index of framebuffer currently being rendered to (for each eye), in range [0, nrFramebuffers-1]

	// VBOs:
	const int N_VAO = 2;
	int curr_vao = 0;
	std::vector<GLuint> VAO, VBO, EBO;   // for mesh
	unsigned int quadVAO, quadVBO = 0; // for copying

	std::vector<Mesh> meshes;
	int nrIndices;
	int nrFrames;
	int currFrame = 0;

	float zeros[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	// for inpuatCameraVisibilityWindow
	bool showCameraVisibilityWindow = false;
	GLuint visibilityVAO, visibilityVBO, visibilityEBO = 0;
	unsigned int* inputCameraIndices = NULL;
	int nrInputCameraIndices = 0;
	float* inputCameraVertexPositions = NULL;
	int offset = 0;

public:

	FrameBufferController() {}
	
	void init(std::vector<InputCamera> inputCameras, unsigned int out_width, unsigned int out_height, Options options) {

		if (options.useVR) {
			nrFramebuffers = 2 * nrFramebuffersPerEye;
		}

		unsigned int in_width = (unsigned int)inputCameras[0].res_x;
		unsigned int in_height = (unsigned int)inputCameras[0].res_y;
		// source: http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-14-render-to-texture/
		glGenFramebuffers(nrFramebuffers, framebuffers);
		glGenTextures(nrFramebuffers, outputTexColors);
		glGenTextures(nrFramebuffers, outputTexAngleAndDepth);
		glGenRenderbuffers(nrFramebuffers, depthrenderbuffers);
		for (int i = 0; i < nrFramebuffers; i++) {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i]);

			// Fragment shader output texture containing intermediary images
			glBindTexture(GL_TEXTURE_2D, outputTexColors[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, out_width, out_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[i], 0);

			// Fragment shader output texture containing intermediary depth
			glBindTexture(GL_TEXTURE_2D, outputTexAngleAndDepth[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, out_width, out_height, 0, GL_RG, GL_FLOAT, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexAngleAndDepth[i], 0);

			// Add a depth test buffer
			//GLuint depthrenderbuffer;
			//glGenRenderbuffers(1, &depthrenderbuffer);
			glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffers[i]);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, out_width, out_height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffers[i]);

			// Set the list of draw buffers.
			GLenum DrawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
			glDrawBuffers(2, DrawBuffers);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				throw std::runtime_error("glCheckFramebufferStatus incorrect");
			}

			glClearColor(options.backgroundColor.r, options.backgroundColor.g, options.backgroundColor.b, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			// clear the second draw buffer seperately
			glClearBufferfv(GL_COLOR, 1, zeros);
		}

		//----------------------------------------
		// load in depth.bin fully
		printf("Reading in %s\n", options.meshPath.c_str());
		std::ifstream in(options.meshPath, std::ios::binary);
		int maxNrIndices = 0;
		int maxNrPositions = 0;
		nrFrames = 0;
		while (true) {
			int n_vertices, n_triangles;
			if (!in.read(reinterpret_cast<char*>(&n_vertices), sizeof(int))) break;
			if (!in.read(reinterpret_cast<char*>(&n_triangles), sizeof(int))) break;

			Mesh m;
			m.vertices.resize(n_vertices * 3);
			m.triangles.resize(n_triangles * 3);

			in.read(reinterpret_cast<char*>(m.vertices.data()), m.vertices.size() * sizeof(float));
			in.read(reinterpret_cast<char*>(m.triangles.data()), m.triangles.size() * sizeof(uint32_t));
			
			if (!in) break;

			maxNrIndices = maxNrIndices < m.triangles.size() ? m.triangles.size() : maxNrIndices;
			maxNrPositions = maxNrPositions < m.vertices.size() ? m.vertices.size() : maxNrPositions;
			meshes.push_back(std::move(m));

			nrFrames++;
		}
		in.close();
		nrFrames = meshes.size();
		printf("Loaded %d meshes\n", nrFrames);
		//printf("maxNrVertices = %d, maxNrTriangles = %d\n", maxNrPositions / 3, maxNrIndices / 3);

		VAO = std::vector<GLuint>(N_VAO);
		VBO = std::vector<GLuint>(N_VAO);
		EBO = std::vector<GLuint>(N_VAO);
		glGenVertexArrays(N_VAO, VAO.data());
		glGenBuffers(N_VAO, VBO.data());
		glGenBuffers(N_VAO, EBO.data());

		for (int i = 0; i < N_VAO; i++) {
			glBindVertexArray(VAO[i]);
			// vertex positions
			glBindBuffer(GL_ARRAY_BUFFER, VBO[i]);
			glBufferData(GL_ARRAY_BUFFER, maxNrPositions * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
			glEnableVertexAttribArray(0);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[i]);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, maxNrIndices * sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
		}

		currFrame = 0;
		curr_vao = 0;
		nrIndices = meshes[0].triangles.size();
		// initialize with first mesh
		UpdateMesh(curr_vao, currFrame);

		//----------------------------------------

		// Setup a quad that fills the screen
		float quadVertices[] = {
			// positions (NDC)  // texCoords
			 1.0f,  1.0f,       1.0f, 1.0f,  // 0 top right
			 1.0f, -1.0f,       1.0f, 0.0f,  // 1 bottom right
			-1.0f,  1.0f,       0.0f, 1.0f,  // 3 top left 
			 1.0f, -1.0f,       1.0f, 0.0f,  // 1 bottom right
			-1.0f, -1.0f,       0.0f, 0.0f,  // 2 bottom left
			-1.0f,  1.0f,       0.0f, 1.0f   // 3 top left 
		};

		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

		// for inputCameraVisibilityWindow
		showCameraVisibilityWindow = options.showCameraVisibilityWindow;
		if (showCameraVisibilityWindow) {
			nrInputCameraIndices = 16 * ((int)inputCameras.size() + 1);
			offset = 5 * (int)inputCameras.size();
			inputCameraIndices = new unsigned int[nrInputCameraIndices];
			int t = 0;
			for (int input = 0; input < (inputCameras.size() + 1); input++) {
				inputCameraIndices[t] = input * 5 + 0;
				inputCameraIndices[t + 1] = input * 5 + 1;
				inputCameraIndices[t + 2] = input * 5 + 0;
				inputCameraIndices[t + 3] = input * 5 + 2;
				inputCameraIndices[t + 4] = input * 5 + 0;
				inputCameraIndices[t + 5] = input * 5 + 3;
				inputCameraIndices[t + 6] = input * 5 + 0;
				inputCameraIndices[t + 7] = input * 5 + 4;
				inputCameraIndices[t + 8] = input * 5 + 1;
				inputCameraIndices[t + 9] = input * 5 + 2;
				inputCameraIndices[t + 10] = input * 5 + 2;
				inputCameraIndices[t + 11] = input * 5 + 4;
				inputCameraIndices[t + 12] = input * 5 + 4;
				inputCameraIndices[t + 13] = input * 5 + 3;
				inputCameraIndices[t + 14] = input * 5 + 3;
				inputCameraIndices[t + 15] = input * 5 + 1;
				t += 16;
			}
			std::vector<glm::vec3> temp;
			float s = 0.08f; // determines display size of the cameras in inputCameraVisibilityWindow
			for (auto& input : inputCameras) {
				temp.push_back(input.pos);
				temp.push_back(input.model * glm::vec4(-s, s, -s, 1));
				temp.push_back(input.model * glm::vec4(s, s, -s, 1));
				temp.push_back(input.model * glm::vec4(-s, -s, -s, 1));
				temp.push_back(input.model * glm::vec4(s, -s, -s, 1));
			}
			// output cam
			temp.push_back(glm::vec3(0));
			temp.push_back(glm::vec3(-s, s, -s));
			temp.push_back(glm::vec3(s, s, -s));
			temp.push_back(glm::vec3(-s, -s, -s));
			temp.push_back(glm::vec3(s, -s, -s));
			// to float array
			inputCameraVertexPositions = new float[temp.size() * 3];
			t = 0;
			for (auto& pos : temp) {
				inputCameraVertexPositions[t] = pos.x;
				inputCameraVertexPositions[t + 1] = pos.y;
				inputCameraVertexPositions[t + 2] = pos.z;
				t += 3;
			}

			glGenVertexArrays(1, &visibilityVAO);
			glGenBuffers(1, &visibilityVBO);
			glGenBuffers(1, &visibilityEBO);
			glBindVertexArray(visibilityVAO);

			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, visibilityEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, nrInputCameraIndices * sizeof(unsigned int), inputCameraIndices, GL_STATIC_DRAW);

			glBindBuffer(GL_ARRAY_BUFFER, visibilityVBO);
			glBufferData(GL_ARRAY_BUFFER, temp.size() * 3 * sizeof(float), inputCameraVertexPositions, GL_STATIC_DRAW);
			// inputCamera position attribute
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);
		}
	}

	void ReceivedNewVideoFrame() {
		// switch to the next vao, which already has a pre-loaded mesh ready
		curr_vao = next_vao(curr_vao);

		currFrame = (currFrame + 1) % nrFrames;
		nrIndices = meshes[currFrame].triangles.size();

		// pre-upload the next mesh
		UpdateMesh(next_vao(curr_vao), (currFrame + 1) % nrFrames);
	}

	GLuint getColorTexture(int eyeOffset) {
		return outputTexColors[index[eyeOffset] + (eyeOffset * 3)];
	}

	void renderMesh(GLuint* images, std::vector<GLuint> textures_depth, std::unordered_set<int> inputsToUse, /*out*/ GLuint outColorTex, GLuint outDepthTex) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outColorTex, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outDepthTex, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glClearBufferfv(GL_COLOR, 1, zeros); // clear depth tex with zeros
		glBindVertexArray(VAO[curr_vao]);
		int i = 0;
		for (auto& idx : inputsToUse) {
			glActiveTexture(GL_TEXTURE0 + 2 * i);
			glBindTexture(GL_TEXTURE_2D, images[idx]);
			glActiveTexture(GL_TEXTURE0 + 2 * i + 1);
			glBindTexture(GL_TEXTURE_2D, textures_depth[idx]);
			i+= 1;
		}
		glDrawElements(GL_TRIANGLES, nrIndices, GL_UNSIGNED_INT, 0);
	}

	void renderMesh(int eyeOffset, GLuint* images, std::vector<GLuint> textures_depth, std::unordered_set<int> inputsToUse) {
		index[eyeOffset] = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[eyeOffset] + (eyeOffset * 3)]); 
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[index[eyeOffset]], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexAngleAndDepth[index[eyeOffset]], 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindVertexArray(VAO[curr_vao]);
		int i = 0;
		for (auto& idx : inputsToUse) {
			glActiveTexture(GL_TEXTURE0 + 2 * i);
			glBindTexture(GL_TEXTURE_2D, images[idx]);
			glActiveTexture(GL_TEXTURE0 + 2 * i + 1);
			glBindTexture(GL_TEXTURE_2D, textures_depth[idx]);
			i += 1;
		}
		glDrawElements(GL_TRIANGLES, nrIndices, GL_UNSIGNED_INT, 0);
	}

	void InpaintImage(int eyeOffset, GLuint colorTex, GLuint depthTex) {
		index[eyeOffset] = 0;
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[eyeOffset] + (eyeOffset * 3)]); 
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[index[eyeOffset]], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexAngleAndDepth[index[eyeOffset]], 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, depthTex);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void renderMeshAsDepthTexture(GLuint outputTex) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTex, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexAngleAndDepth[0], 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glBindVertexArray(VAO[curr_vao]);
		glDrawElements(GL_TRIANGLES, nrIndices, GL_UNSIGNED_INT, 0);
	}

	void dilateDepth(GLuint inputTex, GLuint outputTex) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTex, 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexAngleAndDepth[index[0]], 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, inputTex);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void bindCurrentBuffer() {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[index[0]]);
	}

	void drawInputCamera(int index) {
		glBindVertexArray(visibilityVAO);
		glDrawElementsBaseVertex(GL_LINES, 16, GL_UNSIGNED_INT, 0, 5 * index);
	}

	void drawOutputCamera() {
		glDrawElementsBaseVertex(GL_LINES, 16, GL_UNSIGNED_INT, 0, offset);
	}

	void cleanup() {
		glDeleteFramebuffers(nrFramebuffers, framebuffers);
		glDeleteTextures(nrFramebuffers, outputTexColors);
		glDeleteTextures(nrFramebuffers, outputTexAngleAndDepth);
		glDeleteRenderbuffers(nrFramebuffers, depthrenderbuffers);
		glDeleteVertexArrays(N_VAO, VAO.data());
		glDeleteBuffers(N_VAO, VBO.data());
		glDeleteBuffers(N_VAO, EBO.data());
		VAO.clear();
		VBO.clear();
		EBO.clear();
		glDeleteVertexArrays(1, &quadVAO);
		glDeleteBuffers(1, &quadVBO);
		if (showCameraVisibilityWindow) {
			glDeleteVertexArrays(1, &visibilityVAO);
			glDeleteBuffers(1, &visibilityVBO);
			glDeleteBuffers(1, &visibilityEBO);
			delete[] inputCameraVertexPositions;
			delete[] inputCameraIndices;
		}
	}

private:
	int previous(int index) {
		return (index == 0) ? nrFramebuffersPerEye - 1 : index - 1;
	}
	int next(int index) {
		return (index == nrFramebuffersPerEye - 1) ? 0 : index + 1;
	}

	int next_vao(int index) {
		return (index == N_VAO - 1) ? 0 : index + 1;
	}

	void UpdateMesh(int VAO_index, int frame) {

		//printf("UpdateMesh VAO[d], frame %d\n", VAO_index, frame);

		Mesh m = meshes[frame];

		glBindVertexArray(VAO[VAO_index]);

		glBindBuffer(GL_ARRAY_BUFFER, VBO[VAO_index]);
		glBufferSubData(GL_ARRAY_BUFFER, 0, m.vertices.size() * sizeof(float), m.vertices.data());

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[VAO_index]);
		glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, m.triangles.size() * sizeof(uint32_t), m.triangles.data());

		glBindVertexArray(0);
	}
};


#endif
