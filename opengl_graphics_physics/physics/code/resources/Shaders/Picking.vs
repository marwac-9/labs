#version 420

layout (location = 0) in vec3 Position;

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

out vec3 worldPos;

void main()
{
    gl_Position = MVP * vec4(Position, 1.0);
    worldPos = (M * vec4(Position, 1.0)).xyz;
} 