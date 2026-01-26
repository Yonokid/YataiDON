#version 330
in vec2 fragTexCoord;
out vec4 fragColor;
uniform sampler2D texture0;
uniform sampler2D texture1;

void main() {
    vec4 maskColor = texture(texture0, fragTexCoord);
    vec4 rainbowColor = texture(texture1, fragTexCoord);

    if (maskColor.a > 0.0) {
        fragColor = rainbowColor;
    } else {
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
    }
}
