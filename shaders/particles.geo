#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 proj;

void main()
{
    gl_Position = gl_in[0].gl_Position + vec4(-1, -1, 0.0, 0.0);
    gl_Position = proj * gl_Position;
	EmitVertex();
    gl_Position = gl_in[0].gl_Position + vec4(-1, 1, 0.0, 0.0);
	gl_Position = proj * gl_Position;
    EmitVertex();
	gl_Position = gl_in[0].gl_Position + vec4(1, -1, 0.0, 0.0);
	gl_Position = proj * gl_Position;
    EmitVertex();
	gl_Position = gl_in[0].gl_Position + vec4(1, 1, 0.0, 0.0);
	gl_Position = proj * gl_Position;
    EmitVertex();
    EndPrimitive();
}