#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_pos;

layout(set = 0, binding = 0) uniform ProjectionView {
	mat4 proj_view;	
} pv;
layout(set = 1, binding = 0) uniform Model {
	mat4 model;
} m;

void main() {
	gl_Position = pv.proj_view * m.model * vec4(in_position, 1.0f);
}