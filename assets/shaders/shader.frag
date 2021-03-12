#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 1) uniform sampler2D tex_sampler;
layout(set = 1, binding = 2) uniform sampler2D shadow_map;

layout(location = 0) in vec2 in_tex_pos;
layout(location = 1) in vec4 in_frag_pos_light_space;

layout(location = 0) out vec4 out_color;

void main() {
	// Convert X and Y axises from [-1, 1] to [0, 1] range. Note that depth is already in [0, 1] range by default in Vulkan
	vec3 pos = vec3(in_frag_pos_light_space.xy * 0.5f + 0.5f, in_frag_pos_light_space.z);

	if (pos.z > 1.0f)
		pos.z = 1.0f;

	float closest_depth = texture(shadow_map, pos.xy).r;

	out_color = texture(tex_sampler, in_tex_pos);

	float bias = 0.005f;
	if (closest_depth + bias <= pos.z)
		out_color.xyz *= 0.2f;
}