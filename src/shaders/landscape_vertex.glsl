// AFK (c) Alex Holloway 2013
//
// This is the landscape vertex shader.
// It doesn't do anything in particular -- the projection happens
// in the geometry shader.

#version 330

// Input is in vec4 to match the OpenCL 3-vector format.
layout (location = 0) in vec4 Position;
layout (location = 1) in vec4 Vcol;
layout (location = 2) in vec4 Normal;

uniform float yCellMin;
uniform float yCellMax;

out VertexData
{
    vec3 colour;
    vec3 normal;
    bool withinBounds;
} outData;

void main()
{
    gl_Position = vec4(Position.xyz, 1.0);
    outData.colour = Vcol.xyz;
    outData.normal = Normal.xyz;
    outData.withinBounds = (yCellMin <= Position.y && Position.y < yCellMax);
}
