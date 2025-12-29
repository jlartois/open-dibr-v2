#version 330 core
layout(location = 2) out float FragMask;

in vec2 TexCoords;

uniform float width;
uniform float height;
uniform sampler2D depthTex;
uniform sampler2D erodedMaskTex;

void main()
{
	// check for different mask values
	float mask_c = texture(erodedMaskTex, TexCoords).r;
	for(int y = -3; y <= 3; y++){
		for(int x = -3; x <= 3; x++){
			if(x == 0 && y == 0) continue;
			vec2 coordsNeighbor = TexCoords + vec2(x / width, y / height);
			float mask_n = texture(erodedMaskTex, coordsNeighbor).r;
			if(abs(mask_c - mask_n) > 0.1f) {

				// the surrounding pixels contain different mask values
				FragMask = 1;
				return;
			}
		}
	}

	// check for large depth differences
	vec2 TexCoord = vec2(TexCoords.x, 1 - TexCoords.y);
	float depth_c = texture(depthTex, TexCoord).r;
	for(int y = -3; y <= 3; y++){
		for(int x = -3; x <= 3; x++){
			if(x == 0 && y == 0) continue;
			vec2 coordsNeighbor = TexCoord + vec2(x / width, y / height);
			float depth_n = texture(depthTex, coordsNeighbor).r;
			if(abs(depth_c - depth_n) > 0.01f) {  // TODO user-defined threshold

				// the surrounding pixels have different depths
				FragMask = 1;
				return;
			}
		}
	}

	// the surrounding pixels contain no depth edges or different mask values
	FragMask = 0;
}