/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

// Shape geometry shader.

#version 400

layout (lines_adjacency) in;
layout (invocations = 6) in; /* one for each face */
layout (triangle_strip) out;
layout (max_vertices = 4) out;

// TODO: Here and elsewhere (but it's especially obvious here),
// how about trying to index only the nearest-sampled textures offset by
// +0.5 (AFK_SAMPLE_WIGGLE) and not the linear-sampled textures?
// (Remove the wiggle from the C++ source, and instead apply it here and
// in the other shaders as required?)

// This is the displacement jigsaw texture.
// We sample (x, y, z, w).
uniform sampler2D JigsawDispTex;

// ...and the normal -- let's try using the vapour normal ...
uniform sampler3D JigsawNormalTex;

// ...and the overlap information.
uniform usampler2D JigsawOverlapTex;

// This is the entity display queue.  There are five texels
// per instance, which contain:
// - first 4: the 4 rows of the transform matrix for the instance
// - fifth: (x, y) are the (s, t) jigsaw co-ordinates.
uniform samplerBuffer DisplayTBO; 

// This is the size of an individual vapour jigsaw piece
// in (s, t, r) co-ordinates.
uniform vec3 VapourJigsawPiecePitch;

// This is the size of an individual edge jigsaw piece
// in (s, t) co-ordinates.
uniform vec2 EdgeJigsawPiecePitch;

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


// This function re-scales an edge piece co-ordinate (0-1 within
// the piece) to be a vapour piece co-ordinate (0-1) within a piece
// that has +1 overlap all the way round, as defined by TDIM).
float rescaleEToT(float e)
{
    return (e * EDIM + 1.0) / TDIM;
}

void emitShapeVertex(
    mat4 ClipTransform,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    vec2 edgeJigsawPieceCoord,
    vec2 texCoordDisp,
    int i)
{
    vec2 edgeJigsawCoord = edgeJigsawPieceCoord + EdgeJigsawPiecePitch * (inData[i].texCoord + texCoordDisp);
    float edgeStepsBack = textureLod(JigsawOverlapTex, edgeJigsawCoord, 0).y;

    vec3 vapourTexCoord;
    switch (gl_InvocationID)
    {
    case 0:
        vapourTexCoord = vec3(
            rescaleEToT(inData[i].texCoord.x),
            (edgeStepsBack + 1.0) / TDIM,
            rescaleEToT(inData[i].texCoord.y));
        break;

    case 1:
        vapourTexCoord = vec3(
            (edgeStepsBack + 1.0) / TDIM,
            rescaleEToT(inData[i].texCoord.x),
            rescaleEToT(inData[i].texCoord.y));
        break;

    case 2:
        vapourTexCoord = vec3(
            rescaleEToT(inData[i].texCoord.x),
            rescaleEToT(inData[i].texCoord.y),
            (edgeStepsBack + 1.0) / TDIM);
        break;

    case 3:
        vapourTexCoord = vec3(
            rescaleEToT(inData[i].texCoord.x),
            rescaleEToT(inData[i].texCoord.y),
            ((VDIM - edgeStepsBack) + 1.0) / TDIM);
        break;

    case 4:
        vapourTexCoord = vec3(
            ((VDIM - edgeStepsBack) + 1.0) / TDIM,
            rescaleEToT(inData[i].texCoord.x),
            rescaleEToT(inData[i].texCoord.y));
        break;

    default:
        vapourTexCoord = vec3(
            rescaleEToT(inData[i].texCoord.x),
            ((VDIM - edgeStepsBack) + 1.0) / TDIM,
            rescaleEToT(inData[i].texCoord.y));
        break;
    }

    vec3 vapourJigsawCoord = vapourJigsawPieceCoord + VapourJigsawPiecePitch * vapourTexCoord;

    vec4 dispPosition = textureLod(JigsawDispTex, edgeJigsawCoord, 0);
    gl_Position = ClipTransform * dispPosition;

    //vec4 normal = textureLod(JigsawNormalTex, edgeJigsawCoord, 0);
    vec4 normal = textureLod(JigsawNormalTex, vapourJigsawCoord, 0);
    outData.normal = (WorldTransform * normal).xyz;
    outData.jigsawCoord = edgeJigsawCoord; /* TODO that'll want changing! */
    EmitVertex();
}

void emitShapeSingleTriangle(
    mat4 ClipTransform,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    vec2 edgeJigsawPieceCoord,
    vec2 texCoordDisp,
    int i,
    int j,
    int k,
    int face)
{
    switch (face)
    {
    case 1: case 2: case 5:
        /* Flip this triangle over. */
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, k);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, j);
        break;

    default:
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, j);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, k);
        break;
    }
}

void emitShapeTrianglePair(
    mat4 ClipTransform,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    vec2 edgeJigsawPieceCoord,
    vec2 texCoordDisp,
    int i,
    int j,
    int k,
    int l,
    int face)
{
    switch (face)
    {
    case 1: case 2: case 5:
        /* Flip this triangle over. */
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, k);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, j);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, l);
        break;

    default:
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, j);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, k);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, l);
        break;
    }
}

void main()
{
    /* Reconstruct this instance's jigsaw piece coords.
     */
    vec3 vapourJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 4).xyz;
    vec2 edgeJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 5).xy;

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

    uint overlap = textureLod(JigsawOverlapTex,
        edgeJigsawPieceCoord + EdgeJigsawPiecePitch * (inData[0].texCoord + texCoordDisp),
        0).x;

    /* Check whether this triangle pair is overlapped to another
     * face.
     * I know that the adjacency for the edges is wrong right now:
     * I need to expand their dimensions from eDim to tDim so that
     * they can include correct normal and colour overlap info.)
     */
    if (overlap != 0)
    {
        /* Reconstruct the world transform matrix that I
         * now want ...
         */
        vec4 WTRow1 = texelFetch(DisplayTBO, inData[0].instanceId * 6);
        vec4 WTRow2 = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 1);
        vec4 WTRow3 = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 2);
        vec4 WTRow4 = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 3);

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
            emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, 0, 1, 2, gl_InvocationID);
            break;
            
        case 2:
            emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, 1, 3, 2, gl_InvocationID);
            break;

        case 3:
            emitShapeTrianglePair(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, 0, 1, 2, 3, gl_InvocationID);
            break;

        case 5:
            emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, 2, 0, 3, gl_InvocationID);
            break;

        case 6:
            emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, 3, 0, 1, gl_InvocationID);
            break;

        case 7:
            emitShapeTrianglePair(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, 2, 0, 3, 1, gl_InvocationID);
            break;
        }

        EndPrimitive();
    }
}

