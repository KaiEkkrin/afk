// Test vertex shader.
// TODO currently using one from an Internet example,
// swap this for a real thing!

#version 330

layout (location = 0) in vec3 Position;

uniform mat4 rotate;
uniform mat4 translate;

void main()
{
    gl_Position = rotate * vec4(Position, 1.0);
    gl_Position = translate * gl_Position;
}

