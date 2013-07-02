// Uses colour generated in the vertex shader.

#version 330

uniform vec3 fixedColor;

in vec4 colour;
out vec4 FragColor;

void main()
{
    FragColor = colour;
}

