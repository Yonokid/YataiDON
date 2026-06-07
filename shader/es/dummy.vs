#version 300 es
precision mediump float;

in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matView;
uniform mat4 matProjection;

out vec2 fragTexCoord;
out vec4 fragColor;

void main()
{
    gl_Position = mvp * vec4(vertexPosition, 1.0);
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
}
