// AFK (c) Alex Holloway 2013
//
// This is the landscape vertex shader.
// It doesn't do anything in particular -- the projection happens
// in the geometry shader.

#version 330

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoord;

// This is the jigsaw.  It's a float4: (3 colours, y displacement).
// TODO: This is rather non-optimal storage.  I need the full range
// of a 32-bit float for the y displacement, but the colours would
// be fine as 8-bit normalized.  Therefore, it might be worthwhile
// splitting this into a colour jigsaw (8-bit normalized RGB) and
// a y jigsaw (32-bit float R).
uniform sampler2D JigsawTex;

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

    outData.jigsawCoord = jigsawSTAndYBounds.xy + (JigsawPiecePitch * TexCoord);
    vec4 jigsawTexel = textureLod(JigsawTex, outData.jigsawCoord, 0);

    // Apply the y displacement now.  The rest is for the fragment
    // shader.
    vec3 dispPosition = vec3(Position.x, Position.y + jigsawTexel.w, Position.z);
    gl_Position = vec4(dispPosition * cellCoord.w + cellCoord.xyz, 1.0);

    // Temporary values while I test the basics :).
    //outData.withinBounds = (jigsawSTAndYBounds.z <= Position.y && Position.y < jigsawSTAndYBounds.w);
    outData.withinBounds = true;
}
