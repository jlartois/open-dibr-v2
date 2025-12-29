#ifndef TEX_CONTROLLER_H
#define TEX_CONTROLLER_H

class TexController {
private:

	// video decoding
	std::vector<CUgraphicsResource*> glGraphicsResources;
	std::vector<FFmpegDemuxer*> demuxers;
	std::vector<NvDecoder*> decoders;
	CUcontext* cuContext = NULL;

public:
	TexController() {
		cuContext = new CUcontext();
	}
	
	std::vector<GLuint> images;
	std::vector<GLuint> depths;
	std::vector<GLuint> masks;
	std::vector<GLuint> masks_eroded;
	std::vector<GLuint> masks_dilated;
	std::vector<GLuint> edges;

	bool Init(std::vector<InputCamera> inputCameras, bool verbose) {
		images = std::vector<GLuint>(inputCameras.size());
		depths = std::vector<GLuint>(inputCameras.size());
		masks = std::vector<GLuint>(inputCameras.size());
		masks_eroded = std::vector<GLuint>(inputCameras.size());
		masks_dilated = std::vector<GLuint>(inputCameras.size());
		edges = std::vector<GLuint>(inputCameras.size());
		glGenTextures((GLsizei)inputCameras.size(), images.data());
		glGenTextures((GLsizei)inputCameras.size(), depths.data());
		glGenTextures((GLsizei)inputCameras.size(), masks.data());
		glGenTextures((GLsizei)inputCameras.size(), masks_eroded.data());
		glGenTextures((GLsizei)inputCameras.size(), masks_dilated.data());
		glGenTextures((GLsizei)inputCameras.size(), edges.data());

		for (int i = 0; i < inputCameras.size(); i++) {
			// create mask and edge maps
			glDefineTexture(masks[i], GL_R8, inputCameras[0].res_x, inputCameras[0].res_y, GL_RED, GL_UNSIGNED_BYTE);
			glDefineTexture(masks_eroded[i], GL_R8, inputCameras[0].res_x, inputCameras[0].res_y, GL_RED, GL_UNSIGNED_BYTE);
			glDefineTexture(masks_dilated[i], GL_R8, inputCameras[0].res_x, inputCameras[0].res_y, GL_RED, GL_UNSIGNED_BYTE);
			glDefineTexture(edges[i], GL_R8, inputCameras[0].res_x, inputCameras[0].res_y, GL_RED, GL_UNSIGNED_BYTE);
		}

		int luma_height = inputCameras[0].res_y;
		int luma_height_rounded = ((luma_height + 16 - 1) / 16) * 16; //round luma height up to multiple of 16
		int texture_height = luma_height_rounded + /*chroma height */luma_height / 2;

		ck(cuInit(0));
		
		CUdevice cuDevice = 0;
		ck(cuDeviceGet(&cuDevice, 0));
		char szDeviceName[80];
		ck(cuDeviceGetName(szDeviceName, sizeof(szDeviceName), cuDevice));
		std::cout << "GPU in use: " << szDeviceName << std::endl;
		ck(cuCtxCreate(cuContext, 0, cuDevice));
		
		ck(cuCtxSetCurrent(*cuContext));
		
		for (int i = 0; i < inputCameras.size(); i++) {
			glDefineTexture(images[i], GL_R8, inputCameras[0].res_x, texture_height, GL_RED, GL_UNSIGNED_BYTE);
			glDefineTexture(depths[i], GL_R16, inputCameras[0].res_x, inputCameras[0].res_y, GL_RED, GL_UNSIGNED_SHORT);
		
			// register OpenGL textures for Cuda interop
			CUgraphicsResource* glGraphicsResource_color = new CUgraphicsResource();
			CUgraphicsResource* glGraphicsResource_depth = new CUgraphicsResource();
			ck(cuGraphicsGLRegisterImage(glGraphicsResource_color, images[i], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
			ck(cuGraphicsGLRegisterImage(glGraphicsResource_depth, depths[i], GL_TEXTURE_2D, CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD));
			ck(cuGraphicsResourceSetMapFlags(*glGraphicsResource_color, CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD));
			ck(cuGraphicsResourceSetMapFlags(*glGraphicsResource_depth, CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD));
			glGraphicsResources.push_back(glGraphicsResource_color);
			glGraphicsResources.push_back(glGraphicsResource_depth);
		
			// initialize the LibAV demuxers
			if (verbose) {
				printf("color path %d: %s\n", i, inputCameras[i].pathColor.c_str());
				printf("depth path %d: %s\n", i, inputCameras[i].pathDepth.c_str());
			}
			FFmpegDemuxer* demuxer_color = new FFmpegDemuxer(inputCameras[i].pathColor.c_str(), i == 0);
			FFmpegDemuxer* demuxer_depth = new FFmpegDemuxer(inputCameras[i].pathDepth.c_str());
			demuxers.push_back(demuxer_color);
			demuxers.push_back(demuxer_depth);
		
			// initalize the Cuda Decoders
			NvDecoder* decoder_color = new NvDecoder(cuContext, glGraphicsResource_color, true, FFmpeg2NvCodecId(demuxer_color->GetVideoCodec()), i == 0);
			NvDecoder* decoder_depth = new NvDecoder(cuContext, glGraphicsResource_depth, false, FFmpeg2NvCodecId(demuxer_depth->GetVideoCodec()));
			decoders.push_back(decoder_color);
			decoders.push_back(decoder_depth);
		}
		ck(cuCtxPopCurrent(NULL));

		// prepare the decoders
		for (int i = 0; i < demuxers.size(); i++) {
			// Note: for some reason, we have to decode once before actually decoding the first frame
			int nVideoBytes = 0;
			uint8_t* pVideo = NULL;
			if (!demuxers[i]->Demux(&pVideo, &nVideoBytes)) {
				std::cout << "Error: demuxing failed for input " << i / 2 << (i % 2 == 0 ? " color" : " depth") << std::endl;
				return false;
			}
			decoders[i]->Decode(pVideo, nVideoBytes);
		}

		return true;
	}

	bool DecodeNextVideoFrame() {
		for (int i = 0; i < demuxers.size(); i++) {
			int nVideoBytes = 0;
			uint8_t* pVideo = NULL;
			if (!demuxers[i]->Demux(&pVideo, &nVideoBytes)) {
				std::cout << "Error: demuxing failed for input " << i / 2 << (i % 2 == 0 ? " color" : " depth") << std::endl;
				return false;
			}
			decoders[i]->Decode(pVideo, nVideoBytes);
			// memcopy decoded image to CUGragpicsResources
			decoders[i]->HandlePictureDisplay(decoders[i]->picture_index);
		}
		return true;
	}

	void Cleanup() {
		for (auto& glGraphicsResource : glGraphicsResources) {
			ck(cuGraphicsUnregisterResource(*glGraphicsResource));
			delete glGraphicsResource;
		}
		glGraphicsResources.clear();
		for (auto& demuxer : demuxers) {
			delete demuxer;
		}
		demuxers.clear();
		for (auto& decoder : decoders) {
			delete decoder;
		}
		decoders.clear();
		if (cuContext) {
			ck(cuCtxDestroy(*cuContext));
			delete cuContext;
		}

		glDeleteTextures(images.size(), images.data());
		glDeleteTextures(depths.size(), depths.data());
		glDeleteTextures(masks.size(), masks.data());
		glDeleteTextures(masks_eroded.size(), masks_eroded.data());
		glDeleteTextures(masks_dilated.size(), masks_dilated.data());
		glDeleteTextures(edges.size(), edges.data());
		images.clear();
		depths.clear();
		masks.clear();
		masks_eroded.clear();
		masks_dilated.clear();
		edges.clear();
	}

private:
	static void glDefineTexture(GLuint tex, GLint internalformat, int w, int h, GLenum format, GLenum type, void* data = NULL) {
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, internalformat, w, h, 0, format, type, data == NULL ? 0 : data);
	}
};


#endif
