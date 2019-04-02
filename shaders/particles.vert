#version 330 core

layout (location = 0) in vec4 position;
layout (location = 1) in vec4 velocity;
layout (location = 2) in vec4 color;

out vec4 vVelocity;
out vec4 vColor;

uniform mat4 proj;
uniform mat4 view;

void main()
{
	gl_Position = /*proj*/view*position;
	vColor = color;
	vVelocity = velocity;
	vColor = gl_Position;
}