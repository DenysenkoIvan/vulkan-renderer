#version 450

layout(location = 0) in vec3 in_tex_coords;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D equirectangular_map;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main()
{		
    vec2 uv = SampleSphericalMap(normalize(in_pos));
    vec3 color = texture(equirectangular_map, uv).rgb;
    
    out_color = vec4(color, 1.0);
}