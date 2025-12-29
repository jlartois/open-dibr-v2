#version 330 core
layout(location = 0) out vec4 FragColor;
layout(location = 1) out float Fragdepth;

in vs_out
{
	float outputDepth;
	vec3 worldPosition;
	flat int order[4];
}frag;

uniform float width;
uniform float height;

uniform float out_width;
uniform float out_height;
uniform float chroma_offset;

uniform float isYCbCr;
uniform float convertYCbCrToRGB;

uniform int nrTextures;
uniform sampler2D colorTex[4];
uniform sampler2D depthTex[4];
uniform mat4 in_view[4];
uniform vec2 in_f[4];       // for perspective unprojection
uniform vec2 in_pp[4];      // for perspective unprojection


void main()
{
	Fragdepth = frag.outputDepth;

	// project worldPosition to input camera
	for(int t = 0; t < nrTextures; t++){
		// process colorTexs in order of best (outputCamera, worldPosition, inputCamera) angle
		int i = frag.order[t];

		vec4 viewPosition = in_view[i] * vec4(frag.worldPosition, 1);
		viewPosition = viewPosition / viewPosition.w; 

		if(viewPosition.z >= 0) continue; // behind camera

		float u = -viewPosition.x / viewPosition.z * in_f[i].x + in_pp[i].x;  // column
		float v =  viewPosition.y / viewPosition.z * in_f[i].y + in_pp[i].y;  // row
		vec2 uv = vec2(u / width, v / height);
		
		// check if not occluded in input camera i
		float depth = texture(depthTex[i], vec2(uv.x, 1 - uv.y)).r;
		if (abs(depth + viewPosition.z) > 0.05f) continue; // TODO threshold
		
		if(u > 0 && u < width && v > 0 && v < height){
			// color tex is YUV NV12
			vec2 texcoord_Y = vec2(uv.x, uv.y * height/(height*1.5f + chroma_offset));
			float Cb_x = (floor(floor(uv.x * width) / 2.0f) * 2.0f + 0.5f) / width;
			float Cr_x = (floor(floor(uv.x * width) / 2.0f) * 2.0f + 1.5f) / width;
			float Cb_Cr_y = (floor(floor(uv.y * height) / 2.0f) + 0.5f + height + chroma_offset) / (height * 1.5f + chroma_offset);
		
			float Y = texture(colorTex[i], texcoord_Y).r;
			float Cb = texture(colorTex[i], vec2(Cb_x, Cb_Cr_y)).r;
			float Cr = texture(colorTex[i], vec2(Cr_x, Cb_Cr_y)).r;
		
			if(convertYCbCrToRGB > 0.5f) {
				float r = Y + 1.370705*(Cr - 128.0f / 255.0f);
				float g = Y - 0.698001*(Cr - 128.0f / 255.0f) - 0.337633*(Cb - 128.0f / 255.0f);
				float b = Y + 1.732446*(Cb - 128.0f / 255.0f);
				FragColor = vec4(r, g, b, 1);
			}
			else {
				FragColor = vec4(Y, Cb, Cr, 1);
			}
			return; // TODO blending
		}
	}
	discard;
}