#version 330

in vec4 px_normal;
in vec4 px_color;

out vec4 out_color;

void main(void)
{
    float shade = (px_normal.z + 1.0f) * 2.0f;
    out_color = px_color * shade;
}