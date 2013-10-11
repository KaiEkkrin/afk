// Vertex colour and ambient and diffuse lighting.

#version 330

// Input is in vec4 to match the OpenCL 3-vector format.
layout (location = 0) in vec4 Position;
layout (location = 1) in vec4 Vcol;
layout (location = 2) in vec4 Normal;

uniform mat4 WorldTransform;
uniform mat4 ClipTransform;

out vec3 VcolF;
out vec3 NormalF;

void main()
{
    gl_Position = ClipTransform * vec4(Position.xyz, 1.0);
    VcolF = Vcol.xyz;
    NormalF = (WorldTransform * vec4(Normal.xyz, 0.0)).xyz;
}

