#version 330 core

out vec4 outColor;

in vec4 gVelocity;
in vec4 gColor;

in vec2 gTexCoords;

void main()
{
    outColor = vec4(1.0, 0.0, 0.0, 1.0);
	//outColor = vVelocity * 20;
	//outColor = vec4(gTexCoords, 0.0, 1.0);
	outColor = gColor;
	float absx = abs(gTexCoords.x * 2.0 - 1.0);
	float absy = abs(gTexCoords.y * 2.0 - 1.0);
	outColor.a = 1.0 - (absx*absx + absy*absy);
}  