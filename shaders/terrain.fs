#version 330 core
out vec4 FragColor;

in vec2 vTexCoord;

uniform sampler2D uTextureTerrain;

void main()
{
    FragColor = texture(uTextureTerrain, vTexCoord);
}
