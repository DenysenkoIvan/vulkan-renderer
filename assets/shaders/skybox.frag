#version 450

layout(location = 0) in vec3 in_tex_coords;

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform samplerCube skybox;

void main() {
	out_color = texture(skybox, in_tex_coords);
}