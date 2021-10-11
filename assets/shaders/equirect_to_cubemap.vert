#version 450

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec3 out_tex_coords;

layout(push_constant) uniform Projection {
	layout(offset = 0)

	mat4 proj;
	mat4 view;
};

void main() {
	out_pos = in_pos.xzy;
	out_pos.y *= -1;
	
	gl_Position = proj * view * vec4(in_pos, 1.0f);
}