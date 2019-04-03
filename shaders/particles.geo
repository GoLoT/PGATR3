#version 330 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

uniform mat4 proj;

uniform float particleRadius;

out vec2 gTexCoords;

in vec4 vVelocity[];
in vec4 vColor[];

out vec4 gVelocity;
out vec4 gColor;

void main()
{
    gl_Position = gl_in[0].gl_Position + vec4(-particleRadius, -particleRadius, 0.0, 0.0);
    gl_Position = proj * gl_Position;
	gTexCoords = vec2(0.0, 0.0);
	gVelocity = vVelocity[0];
	gColor = vColor[0];
	EmitVertex();
    gl_Position = gl_in[0].gl_Position + vec4(-particleRadius, particleRadius, 0.0, 0.0);
	gl_Position = proj * gl_Position;
	gTexCoords = vec2(0.0, 1.0);
	gVelocity = vVelocity[0];
	gColor = vColor[0];
    EmitVertex();
	gl_Position = gl_in[0].gl_Position + vec4(particleRadius, -particleRadius, 0.0, 0.0);
	gl_Position = proj * gl_Position;
	gTexCoords = vec2(1.0, 0.0);
	gVelocity = vVelocity[0];
	gColor = vColor[0];
    EmitVertex();
	gl_Position = gl_in[0].gl_Position + vec4(particleRadius, particleRadius, 0.0, 0.0);
	gl_Position = proj * gl_Position;
	gTexCoords = vec2(1.0, 1.0);
	gVelocity = vVelocity[0];
	gColor = vColor[0];
    EmitVertex();
    EndPrimitive();
}