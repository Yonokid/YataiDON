$input v_texcoord0, v_color0, v_texindex, v_colortransform

#include <bgfx_shader.sh>
#include "color_transform.sh"

SAMPLER2D(s_sprite0, 0);
SAMPLER2D(s_sprite1, 1);
SAMPLER2D(s_sprite2, 2);
SAMPLER2D(s_sprite3, 3);
SAMPLER2D(s_sprite4, 4);
SAMPLER2D(s_sprite5, 5);
SAMPLER2D(s_sprite6, 6);
SAMPLER2D(s_sprite7, 7);

void main()
{
    vec4 color;
    if (v_texindex < 0.5) color = texture2D(s_sprite0, v_texcoord0);
    else if (v_texindex < 1.5) color = texture2D(s_sprite1, v_texcoord0);
    else if (v_texindex < 2.5) color = texture2D(s_sprite2, v_texcoord0);
    else if (v_texindex < 3.5) color = texture2D(s_sprite3, v_texcoord0);
    else if (v_texindex < 4.5) color = texture2D(s_sprite4, v_texcoord0);
    else if (v_texindex < 5.5) color = texture2D(s_sprite5, v_texcoord0);
    else if (v_texindex < 6.5) color = texture2D(s_sprite6, v_texcoord0);
    else color = texture2D(s_sprite7, v_texcoord0);
    if (v_colortransform.w > 0.5)
    {
        vec3 hsv = rgb2hsv(color.rgb);
        hsv.x = fract(hsv.x + v_colortransform.x);
        hsv.y = clamp(hsv.y * v_colortransform.y, 0.0, 1.0);
        hsv.z = clamp(hsv.z * v_colortransform.z, 0.0, 1.0);
        color.rgb = hsv2rgb(hsv);
    }
    gl_FragColor = color * v_color0;
}
