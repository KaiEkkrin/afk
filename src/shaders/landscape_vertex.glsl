// AFK (c) Alex Holloway 2013
//
// This is the landscape vertex shader.
// It doesn't do anything in particular -- the projection happens
// in the geometry shader.

#version 330

layout (location = 0) in vec4 Position;
layout (location = 1) in vec2 TexCoord;

// This is the jigsaw.  It's a float4: (3 colours, y displacement).
uniform sampler2D JigsawTex;

// This texture provides the base (u, v) of the instance's
// jigsaw piece.
uniform samplerBuffer JigsawPieceTBO;

// This texture contains the details of the tiles to draw.
// Each instance gets 2 consecutive 3-float texels:
// - (x, y, z) of the cell
// - (cell scale, y min, y max).
uniform samplerBuffer CellTBO;

out VertexData
{
    vec2 jigsawCoord;
    bool withinBounds;
} outData;

void main()
{
    vec3 cellLocation = texelFetch(CellTBO, gl_InstanceID * 2);
    vec3 cellScale = texelFetch(CellTBO, gl_InstanceID * 2 + 1);

    vec2 jigsawPiece = texelFetch(JigsawPieceTBO, gl_InstanceID);

    // TODO I expect I'm sampling this incorrectly -- I need to get
    // into texture space.  Read up about this.  This is a
    // placeholder.
    outData.jigsawCoord = outData.jigsawPiece + TexCoord;
    vec4 jigsawTexel = textureLod(JigsawTex, outData.jigsawCoord, 0);

    // Apply the y displacement now.  The rest is for the fragment
    // shader.
    vec3 dispPosition = vec3(Position.x, Position.y + jigsawTexel.w, Position.z);
    gl_Position = vec4(dispPosition * cellScale.x + cellLocation, 1.0);

    // Temporary values while I test the basics :).
    //outData.withinBounds = (cellScale.y <= Position.y && Position.y < cellScale.z);
    outData.withinBounds = true;
}
