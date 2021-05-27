#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_tangent;
layout(location = 3) in vec2 in_uv0;
layout(location = 4) in vec2 in_uv1;

layout(location = 0) out vec2 out_uv0;
layout(location = 1) out vec2 out_uv1;
layout(location = 2) out mat3 out_TBN;

layout(push_constant) uniform ModelMatrix {
	layout(offset = 0)
	
	mat4 model;
} primitive;

layout(set = 0, binding = 0) uniform WorldMatrix {
	mat4 proj_view;	
} world;

void main() {
	vec3 bitangent = cross(in_normal, in_tangent.xyz) * in_tangent.w;

	mat3 model = mat3(transpose(inverse(primitive.model)));
	vec3 T = normalize(model * in_tangent.xyz);
	vec3 N = normalize(model * in_normal);
	vec3 B = normalize(model * bitangent);
	
	gl_Position = world.proj_view * primitive.model * vec4(in_pos, 1.0f);
	out_uv0 = in_uv0;
	out_TBN = mat3(T, B, N);
	out_uv1 = in_uv1;
}