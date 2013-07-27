// AFK (c) Alex Holloway 2013
//
// This is the landscape vertex shader.
// It doesn't do anything in particular -- the projection happens
// in the geometry shader.

#version 330

layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Vcol;
layout (location = 2) in vec3 Normal;

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
    gl_Position = vec4(Position, 1.0);
    outData.colour = Vcol;
    outData.normal = Normal;
    outData.withinBounds = (yCellMin <= Position.y && Position.y < yCellMax);
}
