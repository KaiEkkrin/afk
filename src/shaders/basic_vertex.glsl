// Pretty much the simplest possible vertex shader.

#version 330

layout (location = 0) in vec3 Position;

uniform mat4 transform;

void main()
{
    gl_Position = transform * vec4(Position, 1.0);
}

