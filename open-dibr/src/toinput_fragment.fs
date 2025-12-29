#version 330 core
layout(location = 0) out float FragDepth;

in vs_out
{
	float outputDepth;
}frag;


void main()
{
	FragDepth = frag.outputDepth;
}