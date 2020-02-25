#version 330

uniform sampler2D Texture;

in vec2 px_textureUV;

out vec4 out_color;

void main(void)
{
    out_color = texture2D(Texture, px_textureUV);
}