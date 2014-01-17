/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

// Shape geometry shader.  Produces triangles according to the overlap
// texture.
//
// This shader requires shape_geometry_base.

layout (lines_adjacency) in;
layout (invocations = 6) in; /* one for each face */
layout (triangle_strip) out;
layout (max_vertices = 4) out;

void emitShapeSingleTriangle(
    mat4 ClipTransform,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    vec2 edgeJigsawPieceCoord,
    vec2 texCoordDisp,
    int instanceId,
    uint layer,
    int i,
    int j,
    int k,
    int face)
{
    switch (face)
    {
    case 1: case 2: case 5:
        /* Flip this triangle over. */
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, k);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, j);
        break;

    default:
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, j);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, k);
        break;
    }
}

void emitShapeTrianglePair(
    mat4 ClipTransform,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    vec2 edgeJigsawPieceCoord,
    vec2 texCoordDisp,
    int instanceId,
    uint layer,
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
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, k);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, j);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, l);
        break;

    default:
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, i);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, j);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, k);
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, instanceId, layer, l);
        break;
    }
}

void main()
{
    /* Reconstruct this instance's jigsaw piece coords.
     */
    vec3 vapourJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 7 + 5).xyz;
    vec2 edgeJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 7 + 6).xy;
    vec2 texCoordDisp = getTexCoordDisp();

    uint allOverlap = textureLod(JigsawOverlapTex,
        makeEdgeJigsawCoordNearest(edgeJigsawPieceCoord, inData[0].texCoord + texCoordDisp),
        0).x;
    for (uint layer = 0; layer < LAYERS; ++layer)
    {
        allOverlap = (allOverlap >> (layer * LAYER_BITNESS));
        if (allOverlap == 0) break;
        uint overlap = (allOverlap & ((1<<LAYER_BITNESS)-1));
     
        /* Check whether this triangle pair is overlapped to another
         * face.
         */
        if (overlap != 0)
        {
            mat4 WorldTransform = getWorldTransform(inData[0].instanceId);
            mat4 ClipTransform = ProjectionTransform * WorldTransform;
     
            /* The 3rd bit of overlap controls which way round the triangles
             * should be.
             * If bit 1 of overlap is set we emit the first triangle;
             * if bit 2 is set we emit the second.
             */
            switch (overlap)
            {
            case 1:
                emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, inData[0].instanceId, layer, 0, 1, 2, gl_InvocationID);
                break;
                
            case 2:
                emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, inData[0].instanceId, layer, 1, 3, 2, gl_InvocationID);
                break;
     
            case 3:
                emitShapeTrianglePair(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, inData[0].instanceId, layer, 0, 1, 2, 3, gl_InvocationID);
                break;
     
            case 5:
                emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, inData[0].instanceId, layer, 2, 0, 3, gl_InvocationID);
                break;
     
            case 6:
                emitShapeSingleTriangle(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, inData[0].instanceId, layer, 3, 0, 1, gl_InvocationID);
                break;
     
            case 7:
                emitShapeTrianglePair(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, inData[0].instanceId, layer, 2, 0, 3, 1, gl_InvocationID);
                break;
            }
     
            EndPrimitive();
        }
    }
}

