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

    outData.withinBounds = (cellCoord.y <= dispPosition.y && dispPosition.y <= (cellCoord.y + cellCoord.w));
    //outData.withinBounds = true;

    gl_Position = dispPosition;
}
