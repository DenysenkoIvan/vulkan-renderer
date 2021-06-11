#version 450

layout(location = 0) in vec2 in_tex_pos;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

layout(push_constant) uniform ImageInfo {
	float exposure;
	float gamma;
};

const float A = 0.15;
const float B = 0.50;
const float C = 0.10;
const float D = 0.20;
const float E = 0.02;
const float F = 0.30;
const float W = 11.2;

vec3 Uncharted2_tonemap(vec3 value) {
	return ((value * (A * value + C * B) + D * E) / (value * (A * value + B) + D * F)) - E / F;
}

vec3 tonemap(vec3 color) {
	vec3 result = Uncharted2_tonemap(color * exposure);
	result /= Uncharted2_tonemap(vec3(W));
	return pow(result, vec3(1 / gamma));
}

void main() {
	vec4 color = texture(tex_sampler, in_tex_pos);
	
	out_color = vec4(tonemap(color.rgb), 1.0);
}