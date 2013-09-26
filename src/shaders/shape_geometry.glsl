// AFK (c) Alex Holloway 2013
//
// Shape geometry shader.
// The point of this right now is to cull triangles that shouldn't
// exist (the edge kernel will have written NAN to those vertices.)

#version 400

layout (lines_adjacency) in;
layout (triangle_strip) out;
layout (max_vertices = 4) out;

// This is the displacement jigsaw texture.
// We sample (x, y, z, w).
uniform sampler2D JigsawDispTex;

// ...and the normal
uniform sampler2D JigsawNormalTex;

// This is the entity display queue.  There are five texels
// per instance, which contain:
// - first 4: the 4 rows of the transform matrix for the instance
// - fifth: (x, y) are the (s, t) jigsaw co-ordinates.
uniform samplerBuffer DisplayTBO; 

// This is the size of an individual jigsaw piece
// in (s, t) co-ordinates.
uniform vec2 JigsawPiecePitch;

uniform mat4 ProjectionTransform;

in VertexData
{
    int instanceId;
    vec2 texCoord;
} inData[];

out GeometryData
{
    vec3 normal;
    vec2 jigsawCoord;
} outData;

void main()
{
    /* Reconstruct this instance's jigsaw piece coord.
     */
    vec2 jigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 5 + 4).xy;

    /* Calculate my input jigsaw coord and position here.
     * TODO: Use geometry shader instancing to replicate this
     * across the 6 faces, rather than having the 6 faces in
     * fixed geometry.
     */
    vec2 jigsawCoord[4];
    vec4 dispPosition[4];

    /* TODO Remove the nan check and replace it with a lookup
     * of the overlap texture.
     */
    bool haveNan = false;
    for (int i = 0; i < 4; ++i)
    {
        jigsawCoord[i] = jigsawPieceCoord + JigsawPiecePitch * inData[i].texCoord;
        dispPosition[i] = textureLod(JigsawDispTex, jigsawCoord[i], 0);

        if (isnan(dispPosition[i].x) || isnan(dispPosition[i].y) ||
            isnan(dispPosition[i].z) || isnan(dispPosition[i].w))
        {
            haveNan = true;
        }
    }

    if (!haveNan)
    {
        /* Reconstruct the world transform matrix that I
         * now want ...
         */
        vec4 WTRow1 = texelFetch(DisplayTBO, inData[0].instanceId * 5);
        vec4 WTRow2 = texelFetch(DisplayTBO, inData[0].instanceId * 5 + 1);
        vec4 WTRow3 = texelFetch(DisplayTBO, inData[0].instanceId * 5 + 2);
        vec4 WTRow4 = texelFetch(DisplayTBO, inData[0].instanceId * 5 + 3);

        mat4 WorldTransform = mat4(
            vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
            vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
            vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
            vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

        for (int i = 0; i < 4; ++i)
        {
            gl_Position = (ProjectionTransform * WorldTransform) * dispPosition[i];

            vec4 normal = textureLod(JigsawNormalTex, jigsawCoord[i], 0);
            outData.normal = (WorldTransform * normal).xyz;

            outData.jigsawCoord = jigsawCoord[i];
            EmitVertex();
        }

        EndPrimitive();
    }
}

