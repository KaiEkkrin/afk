// AFK (c) Alex Holloway 2013
//
// Shape geometry shader.
// The point of this right now is to cull triangles that shouldn't
// exist (the edge kernel will have written NAN to those vertices.)

#version 400

layout (lines_adjacency) in;
layout (invocations = 6) in; /* one for each face */
layout (triangle_strip) out;
layout (max_vertices = 4) out;

// This is the displacement jigsaw texture.
// We sample (x, y, z, w).
uniform sampler2D JigsawDispTex;

// ...and the normal
uniform sampler2D JigsawNormalTex;

// ...and the overlap information.
uniform usampler2D JigsawOverlapTex;

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

    // TODO REMOVE DEBUG
    vec4 shaderDebugColour;
} outData;


void emitShapeVertex(mat4 ClipTransform, mat4 WorldTransform, vec2 jigsawPieceCoord, vec2 texCoordDisp, int i, vec4 debugColour)
{
    vec2 jigsawCoord = jigsawPieceCoord + JigsawPiecePitch * (inData[i].texCoord + texCoordDisp);
    vec4 dispPosition = textureLod(JigsawDispTex, jigsawCoord, 0);

    gl_Position = ClipTransform * dispPosition;

    vec4 normal = textureLod(JigsawNormalTex, jigsawCoord, 0);
    outData.normal = (WorldTransform * normal).xyz;
    outData.jigsawCoord = jigsawCoord;
    outData.shaderDebugColour = debugColour;
    EmitVertex();
}

void main()
{
    /* Debug colour that highlights different faces. */
    vec4 debugColour = vec4(1.0, 0.0, 0.0, 0.0);
    switch (gl_InvocationID)
    {
    case 1: debugColour = vec4(0.0, 1.0, 0.0, 0.0); break;
    case 2: debugColour = vec4(0.0, 0.0, 1.0, 0.0); break;
    case 3: debugColour = vec4(1.0, 1.0, 0.0, 0.0); break;
    case 4: debugColour = vec4(1.0, 0.0, 1.0, 0.0); break;
    case 5: debugColour = vec4(0.0, 1.0, 1.0, 0.0); break;
    }

    /* Reconstruct this instance's jigsaw piece coord.
     */
    vec2 jigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 5 + 4).xy;

    /* Displace the texture coord according to the
     * way the faces are lined up in the edge jigsaws.
     * gl_InvocationID 0 means the bottom face (no displacement).
     */
    vec2 texCoordDisp = vec2(0.0, 0.0);
    switch (gl_InvocationID)
    {
    case 1: texCoordDisp = vec2(1.0, 0.0); break;
    case 2: texCoordDisp = vec2(2.0, 0.0); break;
    case 3: texCoordDisp = vec2(0.0, 1.0); break;
    case 4: texCoordDisp = vec2(1.0, 1.0); break;
    case 5: texCoordDisp = vec2(2.0, 1.0); break;
    }

    /* Check whether this triangle pair is overlapped to another
     * face.
     * I know that the adjacency for the edges is wrong right now:
     * I need to expand their dimensions from eDim to tDim so that
     * they can include correct normal and colour overlap info.)
     */
    uint overlap = textureLod(JigsawOverlapTex,
        jigsawPieceCoord + JigsawPiecePitch * (inData[0].texCoord + texCoordDisp),
        0).x;

    if (overlap != 0)
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

        mat4 ClipTransform = ProjectionTransform * WorldTransform;

        /* The 3rd bit of overlap controls which way round the triangles
         * should be.
         * If bit 1 of overlap is set we emit the first triangle;
         * if bit 2 is set we emit the second.
         */
        switch (overlap)
        {
        case 1:
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 0, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 1, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 2, debugColour);
            break;
            
        case 2:
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 1, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 3, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 2, debugColour);
            break;

        case 3:
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 0, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 1, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 2, debugColour);
            emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 3, debugColour);
            break;

        // TODO re-enable the other faces after debugging.
        case 5:
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 2, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 3, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 0, debugColour);
            break;

        case 6:
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 3, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 1, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 0, debugColour);
            break;

        case 7:
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 2, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 3, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 0, debugColour);
            //emitShapeVertex(ClipTransform, WorldTransform, jigsawPieceCoord, texCoordDisp, 1, debugColour);
            break;
        }

        EndPrimitive();
    }
}

