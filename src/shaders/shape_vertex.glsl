// AFK (c) Alex Holloway 2013
//
// Shape vertex shader.  Looks up the world transform of each instance
// in a texture buffer.

#version 330

layout (location = 0) in vec3 Position;
layout (location = 1) in vec2 TexCoord;

// TODO: Here, after I've gotten the basic shape working, the
// jigsaw textures will appear.

// This is the entity display queue.  There are five texels
// per instance, which contain:
// - first 4: the 4 rows of the transform matrix for the instance
// - fifth: (x, y) are the (s, t) jigsaw co-ordinates
uniform samplerBuffer DisplayTBO; 

uniform mat4 ProjectionTransform;

out VertexData
{
    vec2 jigsawCoord;

    // TODO Here's a temporary colour, to make sure that I'm
    // drawing the base shape correctly.
    vec2 tempColourRG;
} outData;

void main()
{
    // Reconstruct the world transform matrix for this instance.
    // Of course, AFK is row-major...  :/
    vec4 WTRow1 = texelFetch(DisplayTBO, gl_InstanceID * 5);
    vec4 WTRow2 = texelFetch(DisplayTBO, gl_InstanceID * 5 + 1);
    vec4 WTRow3 = texelFetch(DisplayTBO, gl_InstanceID * 5 + 2);
    vec4 WTRow4 = texelFetch(DisplayTBO, gl_InstanceID * 5 + 3);

    mat4 WorldTransform = mat4(
        vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
        vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
        vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
        vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

    outData.jigsawCoord = texelFetch(DisplayTBO, gl_InstanceID * 5 + 4).xy;
    outData.tempColourRG = Position.xz;

    gl_Position = (ProjectionTransform * WorldTransform) * vec4(Position, 1.0);
}

