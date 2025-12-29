// 3D warping

#version 330 core
layout (location = 0) in vec3 Position;

out vs_out
{
	float outputDepth;
	vec3 worldPosition;
	flat int order[4]; 
}vertex;

// output camera parameters
uniform vec3 outputCameraPos;
uniform mat4 out_view;
uniform float out_width;
uniform float out_height;
uniform vec2 out_f;
uniform vec2 out_pp;
uniform vec2 out_near_far;
uniform float isVR;
uniform mat4 project;    // only used in VR mode

uniform int nrTextures;
uniform vec3 inputCameraPos[4];


void main()
{
	vertex.worldPosition = Position;

	// project onto the output image
	vec4 viewPosition = out_view * vec4(Position, 1);
	viewPosition = viewPosition / viewPosition.w;
	vertex.outputDepth = length(viewPosition.xyz);

	if(isVR > 0.5f){
		 gl_Position = project * viewPosition;
	}
	else if(viewPosition.z < 0){
		float u = -viewPosition.x / viewPosition.z * out_f.x + out_pp.x;
		float v =  viewPosition.y / viewPosition.z * out_f.y + out_pp.y;
		float normalised_depth = (-viewPosition.z - out_near_far.x) / (out_near_far.y - out_near_far.x);
		gl_Position = vec4(2.0f * u / out_width - 1.0f, 1 - 2.0f * v / out_height, normalised_depth, 1.0f);
	}
	else {
		gl_Position = vec4(0,0,-10,1);
	}

	// calculate the angle (outputCamera, worldPosition, inputCamera)
	vec3 PO = outputCameraPos - Position;
	float angles[4];
	for(int i = 0; i < nrTextures; i++){
		vec3 PI = inputCameraPos[i] - Position;
		angles[i] = acos(dot(PO, PI) / length(PO) / length(PI)) / 3.15f;  // divide by 3.15 to scale to [0, 1)
		vertex.order[i] = i;
	}
	// bubble sort
	for(int i = 0; i < nrTextures - 1; i++){
		for (int j = 0; j < nrTextures - i - 1; j++) {
            if (angles[j] > angles[j+1]) {
                int tmp = vertex.order[j];
                vertex.order[j] = vertex.order[j+1];
                vertex.order[j+1] = tmp;
				float tmpa = angles[j];
				angles[j] = angles[j+1];
				angles[j+1] = tmpa;
            }
        }
	}
}