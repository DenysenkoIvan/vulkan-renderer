#version 450

layout(location = 0) in vec2 in_tex_pos;

layout(location = 0) out float out_color;

layout(set = 0, binding = 0) uniform sampler2D depth_map;

void main() {
	out_color = texture(depth_map, in_tex_pos).r;
}