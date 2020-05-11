#version 330

uniform mat4 g_projectionMatrix;
uniform mat4 g_viewMatrix;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec4 in_color;
layout(location = 3) in mat4 in_worldMatrix;
layout(location = 7) in mat4 in_normalMatrix;

out vec3 px_normal;
out vec4 px_color;

void main(void)
{
    px_normal = mat3(in_normalMatrix) * in_normal;
    px_normal = normalize(px_normal);

    px_color = in_color;

    gl_Position = vec4(in_position, 1.0f);
    gl_Position = in_worldMatrix * gl_Position;
    gl_Position = g_viewMatrix * gl_Position;
    gl_Position = g_projectionMatrix * gl_Position;
}