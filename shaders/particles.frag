#version 330 core

out vec4 outColor;

in vec4 vVelocity;
in vec4 vColor;

void main()
{
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
	//outColor = vVelocity * 20;
}  