// 3D warping

#version 330 core
layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Color;

out vs_out
{
	float outputDepth;
}vertex;

// output camera parameters
uniform mat4 out_view;
uniform float out_width;
uniform float out_height;
uniform vec2 out_f;
uniform vec2 out_pp;
uniform vec2 out_near_far;


void main()
{
	// project onto the output image
	vec4 viewPosition = out_view * vec4(Position, 1);
	viewPosition = viewPosition / viewPosition.w;
	vertex.outputDepth = -viewPosition.z;

	if(viewPosition.z < 0){
		float u = -viewPosition.x / viewPosition.z * out_f.x + out_pp.x;
		float v =  viewPosition.y / viewPosition.z * out_f.y + out_pp.y;
		float normalised_depth = (-viewPosition.z - out_near_far.x) / (out_near_far.y - out_near_far.x);
		gl_Position = vec4(2.0f * u / out_width - 1.0f, 1 - 2.0f * v / out_height, normalised_depth, 1.0f);
	}
	else {
		gl_Position = vec4(0,0,-10,1);
	}
}