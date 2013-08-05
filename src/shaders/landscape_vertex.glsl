// AFK (c) Alex Holloway 2013
//
// This is the landscape vertex shader.
// It doesn't do anything in particular -- the projection happens
// in the geometry shader.

#version 330

layout (location = 0) in vec4 Position;

// This texture contains the (x, y, z, scale) of the cells
// to draw.
uniform samplerBuffer CellCoordTBO;

out VertexData
{
    vec3 colour;
    vec3 normal;
    bool withinBounds;
} outData;

void main()
{
    vec4 cellCoord = texelFetch(CellCoordTBO, gl_InstanceID);
    gl_Position = vec4(Position.xyz * cellCoord.w + cellCoord.xyz, 1.0);

    // Temporary values while I test the basics :).
    outData.colour = cellCoord.xzw;
    outData.normal = vec3(0.0, 1.0, 0.0);
    //outData.withinBounds = (yCellMin <= Position.y && Position.y < yCellMax);
    outData.withinBounds = true;
}
