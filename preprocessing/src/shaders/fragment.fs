#version 330 core
layout(location = 2) out float FragMask;

in fs_in
{
    vec2 TexCoord;
	float angle;
	float outputDepth;
}frag;

uniform float width;
uniform float height;
uniform vec2 out_near_far;

uniform sampler2D neighborDepthTex;


void main()
{

	float depthMain = frag.outputDepth;
	
	vec2 currentTexCoord = vec2(float(gl_FragCoord.x) / width, 1 - float(gl_FragCoord.y) / height);
	float depthNeighbor = texture(neighborDepthTex, currentTexCoord).r;
	depthNeighbor = 1.0 / (1.0f / out_near_far[1] + depthNeighbor * ( 1.0f / out_near_far[0] - 1.0f / out_near_far[1]));
	depthNeighbor = min(depthNeighbor, 1000.0f);
	
	// mask = 0 means unique pixel, mask = 1 means pixel already covered by other view
	if(abs(depthMain - depthNeighbor) > 0.15f){  // TODO user-defined threshold
		discard; // leave FragMask at 0
	}
	else {
		FragMask = 1;
	}
}