#version 330 core

out vec4 FragColor;

in float Height;

vec3 heightToTerrainColor(float h)
{
    // define colors
    vec3 water = vec3(0.0, 0.15, 0.5);
    vec3 sand  = vec3(0.76, 0.68, 0.50);
    vec3 grass = vec3(0.15, 0.6, 0.2);
    vec3 rock  = vec3(0.45, 0.45, 0.45);
    vec3 snow  = vec3(1.0, 1.0, 1.0);

    // blend using smoothstep across ranges
    if (h < 0.4)
        return mix(water, sand, smoothstep(0.0, 0.3, h));
    else if (h < 0.7)
        return mix(sand, grass, smoothstep(0.3, 0.5, h));
    else if (h < 0.85)
        return mix(grass, rock, smoothstep(0.5, 0.75, h));
    else
        return mix(rock, snow, smoothstep(0.75, 1.0, h));
}

void main()
{
    float h = (Height + 32.0) / 64.0;
    h = clamp(h, 0.0, 1.0);

    vec3 color = heightToTerrainColor(h);
    FragColor = vec4(color, 1.0);
}
