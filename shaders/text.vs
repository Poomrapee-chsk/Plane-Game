#version 330 core
layout (location = 0) in vec2 aPos;

uniform vec2 screenSize;

void main()
{
    vec2 clipSpace = (aPos / screenSize) * 2.0 - 1.0;
    clipSpace.y = -clipSpace.y;
    gl_Position = vec4(clipSpace, 0.0, 1.0);
}