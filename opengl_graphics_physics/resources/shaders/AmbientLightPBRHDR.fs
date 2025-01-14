#version 420

layout(binding = 0) uniform sampler2D positionSampler;
layout(binding = 1) uniform sampler2D albedoSampler;
layout(binding = 2) uniform sampler2D normalsSampler;
layout(binding = 3) uniform sampler2D aoRoughnessMetallic;
layout(binding = 4) uniform samplerCube irradianceMap;
layout(binding = 5) uniform samplerCube prefilterMap;
layout(binding = 6) uniform sampler2D   brdfLUT;

layout(std140, binding = 2) uniform CBVars
{
	mat4 VP;
	vec2 screenSize;
	float near;
	float far;
	vec3 cameraPos;
	vec3 cameraUp;
	vec3 cameraRight;
	vec3 cameraForward;
};

// Ouput data
layout(location = 0) out vec4 color;
layout(location = 1) out vec4 brightColor;

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);

const float PI = 3.14159265359;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
	return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
	float a      = roughness*roughness;
	float a2     = a*a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH*NdotH;
	
	float num   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	
	float num   = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	
	return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);

	return ggx1 * ggx2;
}

void main()
{
	vec2 TexCoord = gl_FragCoord.xy / screenSize;
	vec3 WorldPos = texture(positionSampler, TexCoord).xyz;
	vec3 Albedo = pow(texture(albedoSampler, TexCoord).xyz, vec3(2.2));
	vec3 Normal_worldSpace = texture(normalsSampler, TexCoord).xyz;
	vec3 AoRoughnessMetallic = texture(aoRoughnessMetallic, TexCoord).xyz;
	
	float ao = AoRoughnessMetallic.x;
	float roughness = clamp(AoRoughnessMetallic.y, 0.1, 0.9);
	float metallic = clamp(AoRoughnessMetallic.z, 0.1, 0.9);
	
	// Vector that goes from the vertex to the camera
	vec3 EyeDirectionWS = (cameraPos - WorldPos);
	
	vec3 EyeDirection = normalize(EyeDirectionWS);
	
	vec3 Normal = normalize(Normal_worldSpace);
	
	vec3 R = reflect(-EyeDirection, Normal); 
	
	// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
	// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, Albedo, metallic);
	
	//--------------- ambient lighting (we now use IBL as the ambient term) ---------------------
	
	// ambient lighting (we now use IBL as the ambient term)
	vec3 F = fresnelSchlickRoughness(max(dot(Normal, EyeDirection), 0.0), F0, roughness);
	
	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallic;
	
	//diffuse
	vec3 irradiance = texture(irradianceMap, Normal).rgb;
	vec3 diffuseV   = irradiance * Albedo;
	
	//specular
	// sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
	const float MAX_REFLECTION_LOD = 4.0;
	vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
	vec2 brdf = texture(brdfLUT, vec2(max(dot(Normal, EyeDirection), 0.0), roughness)).rg;
	vec3 specularV = prefilteredColor * (F * brdf.x + brdf.y);
	
	//ambient
	vec3 ambientV = (kD * diffuseV + specularV) * ao;
	
	color = vec4(ambientV, 1.0);
	
	float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));
	if (brightness > 1.0)
		brightColor = color;
	else
	brightColor = vec4(0.0, 0.0, 0.0, 1.0);
}