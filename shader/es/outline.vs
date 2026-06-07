#version 300 es
precision mediump float;

in vec3 vertexPosition;
in vec3 vertexNormal;
in vec2 vertexTexCoord;
in vec4 vertexColor;

uniform mat4 mvp;
uniform float outlineThickness;

void main() {
    vec3 extruded = vertexPosition + vertexNormal * outlineThickness;
    gl_Position = mvp * vec4(extruded, 1.0);
}
