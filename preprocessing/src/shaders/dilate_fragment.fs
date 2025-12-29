#version 330 core
layout(location = 2) out float FragMask;

in vec2 TexCoords;

uniform float width;
uniform float height;
uniform sampler2D inputTex;

void main()
{
	for(int y = -2; y <= 2; y++){
		for(int x = -2; x <= 2; x++){
			vec2 coordsNeighbor = TexCoords + vec2(x / width, y / height);
			float mask = texture(inputTex, coordsNeighbor).r;
			if(mask < 0.5f){
				FragMask = 0;
				return;
			}
		}
	}
	FragMask = 1;
}