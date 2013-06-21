// Basic uniform colour fragment shader.

#version 330

uniform vec3 fixedColor;

out vec4 FragColor;

void main()
{
    FragColor = vec4(fixedColor, 1.0);
}

