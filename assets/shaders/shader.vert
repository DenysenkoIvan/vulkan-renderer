#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_tex_pos;

layout(set = 0, binding = 0) uniform PerMesh {
	mat4 model;
} mesh;

layout(set = 1, binding = 0) uniform WorldMatrix {
	mat4 proj_view;	
} world;

layout(set = 1, binding = 1) uniform LightMatrix {
	mat4 proj_view;
} light;

layout(location = 0) out vec2 out_tex_pos;
layout(location = 1) out vec4 out_frag_pos_light_space;

void main() {
	vec4 world_pos = mesh.model * vec4(in_position, 1.0f);

	out_frag_pos_light_space = light.proj_view * world_pos;
	out_tex_pos = in_tex_pos;

	gl_Position = world.proj_view * world_pos;
}