#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform MVP {
	mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_pos;

layout(location = 0) out vec2 out_tex_pos;

void main() {
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(in_position, 1.0f);
	out_tex_pos = in_tex_pos;
}