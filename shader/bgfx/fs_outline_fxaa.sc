$input v_texcoord0, v_color0

// Port of shader/outline_fxaa.fs: dilates the character's alpha into a dark rim, optional tint.
#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);
uniform vec4 texSize;
uniform vec4 outlineThickness;
uniform vec4 tintColor;
uniform vec4 tintAmount;

void main()
{
    vec2 uv = v_texcoord0;
    vec2 texel = 1.0 / texSize.xy;
    vec4 center = texture2D(s_texColor, uv);

    float coverage = 0.0;
    for (int i = 0; i < 16; i++)
    {
        float angle = 6.28318530718 * (float(i) + 0.5) / 16.0;
        vec2 delta = vec2(cos(angle), sin(angle)) * texel * outlineThickness.x;
        coverage = max(coverage, texture2D(s_texColor, uv + delta).a);
    }

    float alpha = max(coverage, center.a);
    if (alpha <= 0.0) discard;
    gl_FragColor = vec4(center.a > 0.0 ? center.rgb * (1.0 - tintAmount.x) + tintColor.rgb : vec3(0.0, 0.0, 0.0), alpha);
}
