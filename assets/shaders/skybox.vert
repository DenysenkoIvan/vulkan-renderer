#version 450

layout(location = 0) in vec3 in_pos;

layout(location = 0) out vec3 out_tex_coords;

layout(set = 0, binding = 0) uniform UBO 
{
	mat4 proj_view;
};

void main() {
	out_tex_coords = vec3(in_pos.x, in_pos.z, in_pos.y);

	vec4 pos = proj_view * vec4(in_pos, 1.0f);
	
	gl_Position = pos.xyww;
}