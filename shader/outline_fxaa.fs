#version 330

in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 texSize;
uniform float outlineThickness;
uniform vec3 tintColor;
uniform float tintAmount;

void main() {
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / texSize;
    vec4 center = texture(texture0, uv);

    float coverage = 0.0;
    for (int i = 0; i < 16; i++) {
        float angle = 6.28318530718 * (float(i) + 0.5) / 16.0;
        vec2 delta = vec2(cos(angle), sin(angle)) * texel * outlineThickness;
        coverage = max(coverage, texture(texture0, uv + delta).a);
    }

    float alpha = max(coverage, center.a);
    if (alpha <= 0.0) discard;
    fragColor = vec4(center.a > 0.0 ? center.rgb * (1.0 - tintAmount) + tintColor : vec3(0.0), alpha);
}
