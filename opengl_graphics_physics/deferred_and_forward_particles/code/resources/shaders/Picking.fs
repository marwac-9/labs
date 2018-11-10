#version 420

layout (location = 0) out uint FragColor;

layout(std140, binding = 0) uniform GBVars
{
	mat4 MVP;					//16		0
								//16		16
								//16		32
								//16		48
	mat4 M;						//16		64
								//16		80
								//16		96
								//16		112
	vec4 MaterialProperties;	//16		128
	vec3 MaterialColor;			//16		144
	vec2 tiling;				//8			160
	uint objectID;				//4			168 + 4 -> 172
};

void main()
{
	FragColor = objectID;
} 