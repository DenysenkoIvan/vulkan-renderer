#version 450

layout(location = 0) in vec2 in_uv0;
layout(location = 1) in vec2 in_uv1;
layout(location = 2) in vec3 in_world_pos;
layout(location = 3) in mat3 in_TBN;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D ao_rough_met_map;
layout(set = 1, binding = 2) uniform sampler2D normal_map;
layout(set = 1, binding = 3) uniform sampler2D emmisive_map;

layout(set = 0, binding = 1) uniform SceneInfo {
	vec3 camera_pos;
	vec3 light_dir;
	vec3 light_color;
	vec3 ambient_color;
};

layout(push_constant) uniform Material {
	layout(offset = 64)

	vec4 base_color_factor;
	vec4 emissive_factor;
	float metallic_factor;
	float roughness_factor;
	int base_color_uv_set;
	int ao_rough_met_uv_set;
	int normals_uv_set;
	int emissive_uv_set;
	float alpha_mask;
	float alpha_cutoff;
	float is_ao_in_rough_met;
};

vec4 get_albedo() {
	vec4 albedo = base_color_factor;
	
	if (base_color_uv_set == 0)
		albedo *= texture(albedo_map, in_uv0);
	else if (base_color_uv_set == 1)
		albedo *= texture(albedo_map, in_uv1);

	return albedo;
}

vec3 get_ao_rough_met() {
	vec3 ao_rough_met = vec3(1.0f, roughness_factor, metallic_factor);
	
	if (ao_rough_met_uv_set == 0)
		ao_rough_met *= texture(ao_rough_met_map, in_uv0).rgb;
	else if (ao_rough_met_uv_set == 1)
		ao_rough_met *= texture(ao_rough_met_map, in_uv1).rgb;

	return ao_rough_met;
}

vec3 get_normal() {
	return normalize(in_TBN * texture(normal_map, normals_uv_set == 0 ? in_uv0 : in_uv1).xyz);
}

const float PI = 3.1415926535;

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
	vec3 world_pos = in_world_pos;
	vec4 albedo = get_albedo();
	vec3 ao_rough_met = get_ao_rough_met();
	vec3 normal = get_normal();

	float roughness = ao_rough_met.g;
	float metallic = ao_rough_met.b;

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo.rgb, metallic);
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

	vec3 Lo = (kD * albedo.rgb / PI + specular) * radiance * NdotL;

	vec3 ambient = vec3(0.35, 0.35, 0.35) * albedo.rgb;
	out_color = vec4(Lo + ambient, albedo.a);
}