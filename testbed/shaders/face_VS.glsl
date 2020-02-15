#version 330

uniform mat4 g_projectionMatrix;
uniform mat4 g_viewMatrix;
uniform mat4 cb_worldMatrix;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec4 in_color;

out vec4 px_color;

void main(void)
{
    px_color = in_color;
    gl_Position = vec4(in_position, 1.0f);
    gl_Position = cb_worldMatrix * gl_Position;
    gl_Position = g_viewMatrix * gl_Position;
    gl_Position = g_projectionMatrix * gl_Position;
}