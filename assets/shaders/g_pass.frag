#version 450

layout(location = 0) in vec2 in_uv0;
layout(location = 1) in vec2 in_uv1;
layout(location = 2) in mat3 in_TBN;

layout(location = 0) out vec4 out_albedo;
layout(location = 1) out vec3 out_ao_rough_met;
layout(location = 2) out vec4 out_normal;
layout(location = 3) out vec4 out_emissive;

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D ao_rough_met_map;
layout(set = 1, binding = 2) uniform sampler2D normal_map;
layout(set = 1, binding = 3) uniform sampler2D emissive_map;

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
	vec4 albedo = base_color_factor * texture(albedo_map, base_color_uv_set == 0 ? in_uv0 : in_uv1);

	return albedo;
}

vec3 get_ao_rough_met() {
	vec3 ao_rough_met = vec3(1.0f, roughness_factor, metallic_factor);
	
	ao_rough_met *= texture(ao_rough_met_map, ao_rough_met_uv_set == 0 ? in_uv0 : in_uv1).rgb;

	return ao_rough_met;
}

vec4 get_normal() {
	vec3 tangent_normal = texture(normal_map, normals_uv_set == 0 ? in_uv0 : in_uv1).xyz * 2.0f - 1.0f;

	vec4 normal;
	if (isnan(in_TBN[0].x))
		normal = vec4(normalize(in_TBN[2]), 1.0f);
	else
		normal = vec4(normalize(in_TBN * tangent_normal), 1.0f);

	return normal;
}

vec4 get_emissive() {
	vec4 emissive = vec4(emissive_factor.rgb, 1.0f);

	if (emissive_uv_set == 0)
		emissive *= vec4(texture(emissive_map, in_uv0).rgb, 1.0f);
	else if (emissive_uv_set == 1)
		emissive *= vec4(texture(emissive_map, in_uv1).rgb, 1.0f);

	return emissive;
}

void main() {
	out_albedo = get_albedo();
	
	if (alpha_cutoff == 1.0f && out_albedo.a < alpha_mask)
		discard;

	out_ao_rough_met = get_ao_rough_met();
	out_normal = get_normal();
	out_emissive = get_emissive();
}