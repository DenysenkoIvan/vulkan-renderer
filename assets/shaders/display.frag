#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

layout(location = 0) in vec2 tex_pos;

layout(location = 0) out vec4 out_color;

void main() {
	out_color = texture(tex_sampler, tex_pos);
}