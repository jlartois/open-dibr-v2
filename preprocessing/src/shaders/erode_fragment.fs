#version 330 core
layout(location = 2) out float FragMask;

in vec2 TexCoords;

uniform float width;
uniform float height;
uniform sampler2D inputTex;

void main()
{
	int nrMaskedPixels = 0;
	for(int y = -1; y <= 1; y++){
		for(int x = -1; x <= 1; x++){
			vec2 coordsNeighbor = TexCoords + vec2(x / width, y / height);
			float mask = texture(inputTex, coordsNeighbor).r;
			if(mask < 0.5f){
				nrMaskedPixels++;
			}
		}
	}
	FragMask = nrMaskedPixels > 7 ? 0 : 1;
}