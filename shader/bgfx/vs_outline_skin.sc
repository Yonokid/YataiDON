$input a_position, a_normal, a_texcoord0, a_color0, a_indices, a_weight
$output v_texcoord0, v_facing

#include <bgfx_shader.sh>
#include "skin.sh"

uniform vec4 outlineParam;
uniform vec4 screenSize;

void main()
{
    vec3 position, normal;
    skinVertex(a_position, a_normal, a_indices, a_weight, position, normal);
    vec4 clip = mul(u_modelViewProj, vec4(position, 1.0));
    vec3 n = normalize(mul(u_modelView, vec4(normal, 0.0)).xyz);
    if (dot(n.xy, n.xy) > 1e-12)
    {
        vec2 px = outlineParam.x * a_color0.g * normalize(n.xy);
        px.y *= sign(u_proj[1][1]);
        clip.xy += px * 2.0 / screenSize.xy * clip.w;
    }
    gl_Position = clip;
    v_texcoord0 = a_texcoord0;
    v_facing = n.z;
}
