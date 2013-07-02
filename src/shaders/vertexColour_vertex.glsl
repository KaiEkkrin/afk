// Makes a gratuitous colour in the vertex shader.

#version 330

layout (location = 0) in vec3 Position;

uniform mat4 transform;
out vec4 colour;

void main()
{
    colour = vec4(clamp(Position, 0.0, 1.0), 1.0);
    gl_Position = transform * vec4(Position, 1.0);
}

