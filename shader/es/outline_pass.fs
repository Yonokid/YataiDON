#version 300 es
precision mediump float;

in vec2 fragTexCoord;
out vec4 fragColor;

uniform sampler2D texture0;
uniform vec2 textureSize;
uniform float outlineThickness;

void main() {
    vec2 uv = fragTexCoord;
    vec2 texel = 1.0 / textureSize;

    vec4 center = texture(texture0, uv);
    if (center.a > 0.001) {
        fragColor = vec4(center.rgb, 1.0);
        return;
    }

    float accum = 0.0;
    vec2 offsets[8] = vec2[8](
        vec2(-1.0,  0.0),
        vec2( 1.0,  0.0),
        vec2( 0.0, -1.0),
        vec2( 0.0,  1.0),
        vec2(-1.0, -1.0),
        vec2( 1.0, -1.0),
        vec2(-1.0,  1.0),
        vec2( 1.0,  1.0)
    );

    for (int i = 0; i < 8; ++i) {
        vec2 sampleUv = uv + offsets[i] * texel * outlineThickness;
        accum += texture(texture0, sampleUv).a;
    }

    if (accum <= 0.0) {
        discard;
    }

    float alpha = clamp(accum / 2.0, 0.0, 1.0);
    fragColor = vec4(0.05, 0.05, 0.05, alpha);
}
