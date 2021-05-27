#version 450

layout(location = 0) in vec2 in_uv;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D albedo_map;
layout(set = 0, binding = 1) uniform sampler2D ao_rough_met_map;
layout(set = 0, binding = 2) uniform sampler2D normal_map;
layout(set = 0, binding = 3) uniform sampler2D emissive_map;
layout(set = 0, binding = 4) uniform sampler2D depth_map;
layout(set = 0, binding = 5) uniform Proj {
	mat4 proj_inv;
};
layout(set = 0, binding = 6) uniform View {
	mat4 view_inv;	
};

layout(push_constant) uniform LightInfo {
	vec3 camera_pos;
	vec3 light_dir;
	vec3 light_color;
	vec3 ambient_color;
};

const float PI = 3.1415926535;

vec3 calculate_world_space_position() {
	float depth = texture(depth_map, in_uv).r;

	vec4 clip_space_pos = vec4(in_uv * 2.0f - 1.0f, depth, 1.0f);
	vec4 view_space_pos = proj_inv * clip_space_pos;
	vec4 world_space_pos = view_inv * view_space_pos;
	
	return world_space_pos.xyz / world_space_pos.w;
}

float distributionGGX(float NdotH, float roughness) {
	float a = roughness * roughness;
	float a2 = a * a;
	float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
	denom = PI * denom * denom;
	return a2 / max(denom, 0.0000001);
}

float geometrySmith(float NdotV, float NdotL, float roughness) {
	float r = roughness + 1.0;
	float k = (r * r) / 8.0;
	float ggx1 = NdotV / (NdotV * (1.0 - k) + k);
	float ggx2 = NdotL / (NdotL * (1.0 - k) + k);
	return ggx1 * ggx2;
}

vec3 fresnelSchlick(float HdotV, vec3 F0) {
	return F0 + (1.0 - F0) * pow(max(1.0 - HdotV, 0.0), 5.0);
}

void main() {
	vec3 world_pos = calculate_world_space_position();
	vec3 albedo = texture(albedo_map, in_uv).rgb;
	vec3 ao_rough_met = texture(ao_rough_met_map, in_uv).rgb;
	vec3 normal = texture(normal_map, in_uv).xyz;

	float roughness = ao_rough_met.g;
	float metallic = ao_rough_met.b;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);
	vec3 radiance = light_color;

	vec3 N = normalize(normal);
	vec3 V = normalize(camera_pos - world_pos);
	vec3 L = normalize(light_dir);
	vec3 H = normalize(V + L);

	float NdotV = max(dot(N, V), 0.0000001);
	float NdotL = max(dot(N, L), 0.0000001);
	float HdotV = max(dot(H, V), 0.0);
	float NdotH = max(dot(N, H), 0.0);

	float D = distributionGGX(NdotH, roughness);
	float G = geometrySmith(NdotV, NdotL, roughness);
	vec3 F = fresnelSchlick(HdotV, F0);

	vec3 specular = D * G * F;
	specular /= 4.0 * NdotV * NdotL;

	vec3 kD = (vec3(1.0) - F) * (1 - metallic);

	vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

	vec3 ambient = vec3(0.35, 0.35, 0.35) * albedo;
	out_color = vec4(Lo + ambient, 1.0f) + texture(emissive_map, in_uv);
}