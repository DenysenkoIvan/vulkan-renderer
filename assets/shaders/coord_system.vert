#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 out_color;

layout(set = 0, binding = 0) uniform ViewProj {
	mat4 proj_view;
} world;

void main() {
	gl_Position =  world.proj_view * vec4(in_pos, 1.0f);
	out_color = in_color;
}