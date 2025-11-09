#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform float blendFactor;

void main()
{
    vec2 inverted_tex_coords = vec2(TexCoords.x, 1.0 - TexCoords.y);

    // 1. Sample both textures
    vec4 color1 = texture(texture_diffuse1, inverted_tex_coords);
    vec4 color2 = texture(texture_diffuse2, inverted_tex_coords);

    // 2. Combine the textures using a linear interpolation (lerp)
    // The blendFactor should range from 0.0 (full color1) to 1.0 (full color2)
    FragColor = mix(color1, color2, blendFactor);
}
