#version 330 core
layout(location = 0) out float FragOut; // TODO

in vec2 TexCoords;

uniform float width;
uniform float height;
uniform sampler2D inputTex;

void main()
{
	float depth_c = texture(inputTex, TexCoords).r;
	float lowest_depth = depth_c;
	int radius = 2;
	for(int y = -radius; y <= radius; y++){
		for(int x = -radius; x <= radius; x++){
			if(x == 0 && y == 0) continue;
			vec2 coordsNeighbor = TexCoords + vec2(x / width, y / height);
			float depth_n = texture(inputTex, coordsNeighbor).r;
			if(depth_n < lowest_depth){
				lowest_depth = depth_n;
			}
		}
	}
	FragOut = lowest_depth; 
}