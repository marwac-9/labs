#version 420 core

in vec3 UVdirection;
layout(location = 0) out vec3 color;
layout(location = 1) out vec3 brightColor;

layout(binding = 0) uniform samplerCube cubeMap;

void main(){
	color = pow(texture(cubeMap, UVdirection).rgb, vec3(2.2));
	float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
	if (brightness > 1.0)
		brightColor = color;
	else
		brightColor = vec3(0.0, 0.0, 0.0);
}