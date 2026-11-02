$input a_position, i_data0, i_data1, i_data2, i_data3
$output v_texcoord0, v_color0, v_texindex, v_colortransform

#include <bgfx_shader.sh>

void main()
{
    vec2 corner = a_position.xy;
    vec2 local = corner * i_data0.zw - i_data2.xy;
    float s = sin(i_data2.z);
    float c = cos(i_data2.z);
    vec2 position = i_data0.xy + vec2(local.x * c - local.y * s, local.x * s + local.y * c);
    gl_Position = mul(u_modelViewProj, vec4(position, 0.0, 1.0));
    v_texcoord0 = mix(i_data1.xy, i_data1.zw, corner);
    uint rg = uint(i_data2.w);
    uint ba = uint(i_data3.w);
    v_color0 = vec4(float(rg & 255u), float((rg >> 8u) & 255u),
                    float(ba & 255u), float((ba >> 8u) & 255u)) / 255.0;
    v_texindex = float((rg >> 16u) & 7u);
    v_colortransform = vec4(i_data3.xyz, float((rg >> 19u) & 1u));
}
