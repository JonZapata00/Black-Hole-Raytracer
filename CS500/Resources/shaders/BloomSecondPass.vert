#version 440 core

layout(location = 0) in vec3 attr_position;
layout(location = 1) in vec2 uv;

out vec2 TexCoords;

void main()
{
    TexCoords = uv;
    gl_Position = vec4(attr_position, 1.0f);
}