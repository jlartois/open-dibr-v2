#ifndef FRAMEBUFFER_CONTROLLER_H
#define FRAMEBUFFER_CONTROLLER_H


class FrameBufferController {
private:
	// FBOs:
	int nrFramebuffers = 1;
	GLuint framebuffers[1];
	GLuint depthrenderbuffers[1];

	// VBOs:
	GLuint VAO, VBO, EBO = 0;          // for 3D warping
	unsigned int quadVAO, quadVBO = 0; // for copying

	// some state:
	int nrIndices = 0;
	unsigned int* indices = NULL;
	float* texCoords = NULL;

public:
	GLuint outputTexColors[1];
	GLuint outputTexDepths[1];
	GLuint outputTexMasks[1];

	FrameBufferController() {}
	
	bool Init(std::vector<InputCamera> inputCameras, Options options) {

		int width = inputCameras[0].res_x;
		int height = inputCameras[0].res_y;

		glGenFramebuffers(nrFramebuffers, framebuffers);
		glGenTextures(nrFramebuffers, outputTexColors);
		glGenTextures(nrFramebuffers, outputTexDepths);
		glGenTextures(nrFramebuffers, outputTexMasks);
		glGenRenderbuffers(nrFramebuffers, depthrenderbuffers);
		for (int i = 0; i < nrFramebuffers; i++) {
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i]);

			glBindTexture(GL_TEXTURE_2D, outputTexColors[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[i], 0);

			glBindTexture(GL_TEXTURE_2D, outputTexDepths[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexDepths[i], 0);

			glBindTexture(GL_TEXTURE_2D, outputTexMasks[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, 0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, outputTexMasks[i], 0);

			// Add a depth test buffer
			glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffers[i]);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffers[i]);

			// Set the list of draw buffers.
			GLenum DrawBuffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
			glDrawBuffers(3, DrawBuffers);

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
				printf("glCheckFramebufferStatus incorrect\n");
				return false;
			}

			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}

		// Setup the trianglemesh that will be drawn (shared by all input cameras)
		nrIndices = 6 * width * height; // 2 triangles per pixel, since pixel is in center of square
		indices = new unsigned int[nrIndices];
		unsigned int t = 0;
		for (int row = 0; row < (width + 1) * height; row += width + 1) {
			for (int col = 0; col < width; col++) {
				indices[t] = row + col;
				indices[t + 1] = row + width + col + 1;
				indices[t + 2] = row + col + 1;
				indices[t + 3] = row + col + 1;
				indices[t + 4] = row + width + col + 1;
				indices[t + 5] = row + width + col + 2;
				t += 6;
			}
		}

		// VBO with texture coordinates
		int nrVertices = (width + 1) * (height + 1);
		texCoords = new float[nrVertices * 2];
		t = 0;
		float width_f = float(width);
		float height_f = float(height);
		for (int row = 0; row < height + 1; row++) {
			for (int col = 0; col < width + 1; col++) {
				texCoords[t] = col / width_f;
				texCoords[t + 1] = row / height_f;
				t += 2;
			}
		}

		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * nrVertices * 2, texCoords, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * nrIndices, indices, GL_STATIC_DRAW);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);


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

		return true;
	}

	void WarpToNeighbor(GLuint mainDepth, GLuint neighborDepth, GLuint neighborMask, bool shouldClear) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, neighborMask, 0);
		if (shouldClear) { 
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
		}
		else {
			glClear(GL_DEPTH_BUFFER_BIT);
		}
		glBindVertexArray(VAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, mainDepth);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, neighborDepth);
		glDrawElements(GL_TRIANGLES, nrIndices, GL_UNSIGNED_INT, 0);
	}

	void ShowMask(GLuint colorTex, GLuint maskTex) {
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, colorTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, maskTex);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void ProcessUInt8Tex(GLuint inputTex, /*out*/ GLuint outputTex) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexDepths[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, outputTex, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, inputTex);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	void CalculateEdgeMask(GLuint depthTex, GLuint erodedMaskTex, /*out*/ GLuint outputTex) {
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, outputTexColors[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, outputTexDepths[0], 0);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, outputTex, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, depthTex);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, erodedMaskTex);
		glBindVertexArray(quadVAO);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}
	

	void cleanup() {
		glDeleteFramebuffers(nrFramebuffers, framebuffers);
		glDeleteTextures(nrFramebuffers, outputTexColors);
		glDeleteTextures(nrFramebuffers, outputTexDepths);
		glDeleteTextures(nrFramebuffers, outputTexMasks);
		glDeleteRenderbuffers(nrFramebuffers, depthrenderbuffers);
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
		delete[] indices;
		delete[] texCoords;
		glDeleteVertexArrays(1, &quadVAO);
		glDeleteBuffers(1, &quadVBO);
	}
};


#endif
