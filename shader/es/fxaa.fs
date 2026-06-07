#version 300 es
precision mediump float;

in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 textureSize;

#define FXAA_SPAN_MAX   8.0
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_REDUCE_MIN (1.0 / 128.0)

void main() {
    vec2 inv = 1.0 / textureSize;
    vec2 uv = fragTexCoord;

    vec3 nw = texture(texture0, uv + vec2(-1.0, -1.0) * inv).rgb;
    vec3 ne = texture(texture0, uv + vec2(+1.0, -1.0) * inv).rgb;
    vec3 sw = texture(texture0, uv + vec2(-1.0, +1.0) * inv).rgb;
    vec3 se = texture(texture0, uv + vec2(+1.0, +1.0) * inv).rgb;
    vec4 m = texture(texture0, uv);

    const vec3 luma = vec3(0.299, 0.587, 0.114);
    float lNW = dot(nw, luma), lNE = dot(ne, luma);
    float lSW = dot(sw, luma), lSE = dot(se, luma);
    float lM = dot(m.rgb, luma);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    vec2 dir = vec2(
            -((lNW + lNE) - (lSW + lSE)),
            ((lNW + lSW) - (lNE + lSE))
        );

    float reduce = max((lNW + lNE + lSW + lSE) * 0.25 * FXAA_REDUCE_MUL, FXAA_REDUCE_MIN);
    float rcpMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);
    dir = clamp(dir * rcpMin, -FXAA_SPAN_MAX, FXAA_SPAN_MAX) * inv;

    vec4 A = 0.5 * (texture(texture0, uv + dir * (1.0 / 3.0 - 0.5)) +
                texture(texture0, uv + dir * (2.0 / 3.0 - 0.5)));
    vec4 B = A * 0.5 + 0.25 * (texture(texture0, uv - dir * 0.5) +
                    texture(texture0, uv + dir * 0.5));

    float lB = dot(B.rgb, luma);
    fragColor = (lB < lMin || lB > lMax) ? A : B;
    fragColor.a = m.a;
}
