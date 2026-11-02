$input a_position, a_normal, a_texcoord0, a_color0, a_indices, a_weight
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>
#include "skin.sh"

void main()
{
    vec3 position, normal;
    skinVertex(a_position, a_normal, a_indices, a_weight, position, normal);
    gl_Position = mul(u_modelViewProj, vec4(position, 1.0));
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
}
