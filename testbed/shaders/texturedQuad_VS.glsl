#version 330

uniform mat4 g_projectionMatrix;
uniform mat4 g_viewMatrix;
uniform mat4 cb_worldMatrix;

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_textureUV;

out vec2 px_textureUV;

void main(void)
{
    gl_Position = vec4(in_position, 0.0f, 1.0f);
    gl_Position = cb_worldMatrix * gl_Position;
    gl_Position = g_viewMatrix * gl_Position;
    gl_Position = g_projectionMatrix * gl_Position;

    px_textureUV = in_textureUV;
}