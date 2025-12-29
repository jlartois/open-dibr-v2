#version 330 core
layout(location = 0) out vec4 FragOut; // TODO

in vec2 TexCoords;

uniform float width;
uniform float height;
uniform sampler2D colorTex;
uniform sampler2D depthTex;

void main()
{
	float depth_c = texture(depthTex, TexCoords).r;

	if(depth_c > 0){
		// no inpainting necessary
		FragOut = texture(colorTex, TexCoords);
		return;
	}

	float highest_depth = 0;
	vec2 best_coords;
	float depth_n;
	vec2 coords;
	for(int radius = 1; radius <= 5; radius++){
		float step_x = radius / width;
		float step_y = radius / height;
		coords = TexCoords + vec2(-step_x, 0);
		depth_n = texture(depthTex, coords).r;
		if(depth_n > highest_depth) {
			best_coords = coords; 
			highest_depth = depth_n;
		}
		
		coords = TexCoords + vec2(step_x, 0);
		depth_n = texture(depthTex, coords).r;
		if(depth_n > highest_depth) {
			best_coords = coords; 
			highest_depth = depth_n;
		}
		
		coords = TexCoords + vec2(0, -step_y);
		depth_n = texture(depthTex, coords).r;
		if(depth_n > highest_depth) {
			best_coords = coords; 
			highest_depth = depth_n;
		}
		
		coords = TexCoords + vec2(0, step_y);
		depth_n = texture(depthTex, coords).r;
		if(depth_n > highest_depth) {
			best_coords = coords; 
			highest_depth = depth_n;
		}
	}

	if(highest_depth > 0) FragOut = texture(colorTex, best_coords);
	else {
		discard;
	}
}