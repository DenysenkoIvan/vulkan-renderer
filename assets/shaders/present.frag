#version 450

layout(location = 0) in vec2 in_tex_pos;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D tex_sampler;

void main() {
	vec4 color = texture(tex_sampler, in_tex_pos);
	
	//color = color / (color + 1.0);
	//color = pow(color, vec4(1.0 / 1.8));

	//out_color = vec4(color.rgb, 1.0);

	out_color = vec4(color.rgb, 1.0);
}