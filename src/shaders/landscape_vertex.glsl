// AFK (c) Alex Holloway 2013
//
// This is the landscape vertex shader.
// It doesn't do anything in particular -- the projection happens
// in the geometry shader.

#version 330

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoord;

// This is the y displacement jigsaw texture.
uniform sampler2D JigsawYDispTex;

// This is the landscape display queue.  It's a float4, and there
// are 2 (consecutive) texels per instance: cell coord, then
// (jigsaw piece s, jigsaw piece t, lower y-bound, upper y-bound)
// as defined in landscape_display_queue.
uniform samplerBuffer DisplayTBO;

// This is the size of an individual jigsaw piece
// in (s, t) co-ordinates.
uniform vec2 JigsawPiecePitch;

out VertexData
{
    vec2 jigsawCoord;
    bool withinBounds;
} outData;

void main()
{
    vec4 cellCoord = texelFetch(DisplayTBO, gl_InstanceID * 2);
    vec4 jigsawSTAndYBounds = texelFetch(DisplayTBO, gl_InstanceID * 2 + 1);

    outData.jigsawCoord = jigsawSTAndYBounds.xy + (JigsawPiecePitch * TexCoord.st);
    float jigsawYDisp = textureLod(JigsawYDispTex, outData.jigsawCoord, 0);

    // Apply the y displacement now.  The rest is for the fragment
    // shader.
    vec4 dispPosition = vec4(
        Position.x * cellCoord.w + cellCoord.x,
        (Position.y + jigsawYDisp) * cellCoord.w,
        Position.z * cellCoord.w + cellCoord.z,
        1.0);

    // TODO The y-bound cell choice isn't working right, no doubt to do with
    // some LoD induced subtlety.  I don't really care, because I'm pretty
    // sure now that I should throw it away and replace it by strictly
    // y-bounded geometry (rebound at top and bottom when calculating,
    // in order to be able to support arbitrary shapes with the same algorithm.
    // But for now, I'm going to apply a bodge in order to draw a landscape
    // without too many holes in.
    outData.withinBounds = ((cellCoord.y - cellCoord.w) <= dispPosition.y && dispPosition.y <= (cellCoord.y + 2.0 * cellCoord.w));
    //outData.withinBounds = true;

    gl_Position = dispPosition;
}
