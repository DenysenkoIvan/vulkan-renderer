#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_tex_pos;

layout(location = 0) out vec2 out_tex_pos;

void main() {
	gl_Position = vec4(in_pos, 0.0f, 1.0f);
	out_tex_pos = in_tex_pos;
}