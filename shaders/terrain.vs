#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;
out float Height;
out vec3 Position;
out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;
uniform sampler2D heightMap;
uniform float yScale;
uniform float yShift;

void main()
{
    Height = aPos.y;
    Position = (view * model * vec4(aPos, 1.0)).xyz;
    FragPos = vec3(model * vec4(aPos, 1.0));
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    vTexCoord = aTexCoord;

    // Compute normal (simplified, assuming flat terrain or simple calculation)
    // For better accuracy, this could be improved with neighboring height samples
    Normal = vec3(0.0, 1.0, 0.0); // Simple up normal
}
