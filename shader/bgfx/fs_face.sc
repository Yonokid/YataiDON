$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 colDiffuse;

void main()
{
    vec4 color = texture2D(s_texColor, v_texcoord0) * colDiffuse * v_color0;
    if (color.a < 0.01) discard;
    gl_FragColor = color;
    gl_FragDepth = gl_FragCoord.z - 0.001;
}
