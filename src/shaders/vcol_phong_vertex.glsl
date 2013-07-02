// Vertex colour and ambient and diffuse lighting.

#version 330

layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Vcol;
layout (location = 2) in vec3 Normal;

uniform mat4 WorldTransform;
uniform mat4 ClipTransform;

out vec3 VcolF;
out vec3 NormalF;

void main()
{
    gl_Position = ClipTransform * vec4(Position, 1.0);
    VcolF = Vcol;
    NormalF = (WorldTransform * vec4(Normal, 0.0)).xyz;
}

