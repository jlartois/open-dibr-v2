#version 330 core
layout(location = 0) out vec4 FragColor;

in vec2 TexCoords;

uniform float height;
uniform float chroma_offset;
uniform sampler2D imageTex;
uniform sampler2D maskTex;

void main()
{
	// color tex is YUV NV12
	vec2 texcoord_Y = vec2(TexCoords.x, (1 - TexCoords.y) * height/(height*1.5f + chroma_offset));
	float color = texture(imageTex, texcoord_Y).r;
		
	float mask = texture(maskTex, TexCoords).r;

	if(mask < 0.5f){
		// keep pixels, so green
		FragColor = vec4(color * 0.5f, color, color * 0.5f, 1);
	}
	else {
		// duplicate pixels, so red
		FragColor = vec4(color, color * 0.5f, color * 0.5f, 1);
	}
}