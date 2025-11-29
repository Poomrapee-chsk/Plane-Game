#version 330 core

out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosLightSpace;

uniform sampler2D texture_diffuse1;
uniform sampler2D texture_diffuse2;
uniform float blendFactor;
uniform sampler2D shadowMap;

uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform float fogDensity;

float ShadowCalculation(vec4 fragPosLightSpace)
{
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r;
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // check whether current frag pos is in shadow
    const float bias = 0.0005f;
    float shadow = currentDepth - bias > closestDepth ? 1.0 : 0.0;

    return shadow;
}

void main()
{
    vec2 inverted_tex_coords = vec2(TexCoords.x, 1.0 - TexCoords.y);

    vec4 color1 = texture(texture_diffuse1, inverted_tex_coords);
    vec4 color2 = texture(texture_diffuse2, inverted_tex_coords);

    vec4 texColor = mix(color1, color2, blendFactor);

    vec3 norm = normalize(Normal);
    vec3 lightDirNorm = normalize(-lightDir); // assuming lightDir is towards the light
    float diff = max(dot(norm, lightDirNorm), 0.0);
    vec3 diffuse = diff * lightColor;

    // ambient
    vec3 ambient = 0.3 * lightColor;

    // shadow
    float shadow = ShadowCalculation(FragPosLightSpace);

    vec3 lighting = (ambient + (1.0 - shadow) * diffuse) * texColor.rgb;

    // Fog calculation
    float distance = length(viewPos - FragPos);
    float fogFactor = 1.0 - exp(-fogDensity * distance);
    vec3 finalColor = mix(lighting, fogColor, fogFactor);

    FragColor = vec4(finalColor, texColor.a);
}
