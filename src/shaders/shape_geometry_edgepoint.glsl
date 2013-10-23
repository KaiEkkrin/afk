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
// This shader ignores overlap and produces one point
// for each edgeStepsBack.  It renders distant shapes quite nicely,
// and I hope to use it to debug the edge layering system.
//
// This shader requires shape_geometry_base.

layout (lines_adjacency) in;
layout (invocations = 6) in; /* one for each face */
layout (points) out;
layout (max_vertices = 1) out;

bool withinView(vec4 clipPosition)
{
    return (abs(clipPosition.x) <= clipPosition.w &&
        abs(clipPosition.y) <= clipPosition.w &&
        abs(clipPosition.z) <= clipPosition.w);
}

vec2 clipToScreenXY(vec4 clipPosition)
{
    /* Make normalized device co-ordinates
     * (between -1 and 1):
     */
    vec2 ndcXY = clipPosition.xy / clipPosition.w;

    /* Now, scale this to the screen: */
    return vec2(
        WindowSize.x * (ndcXY.x + 1.0) / 2.0,
        WindowSize.y * (ndcXY.y + 1.0) / 2.0);
}

void main()
{
    /* Reconstruct this instance's jigsaw piece coords.
     */
    vec3 vapourJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 7 + 5).xyz;
    vec2 edgeJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 7 + 6).xy;
    vec2 texCoordDisp = getTexCoordDisp();

    mat4 WorldTransform = getWorldTransform(inData[0].instanceId);
    mat4 ClipTransform = ProjectionTransform * WorldTransform;

    /* Work out a good size for this point, by pulling the adjacent ones
     * and calculating their distance within the clip transform.
     */
    vec2 basePointEdgeCoord = makeEdgeJigsawCoordNearest(edgeJigsawPieceCoord, inData[0].texCoord + texCoordDisp);
    float edgeStepsBack = textureLod(JigsawESBTex, basePointEdgeCoord, 0);

    /* A negative edge steps back means no edge was detected here. */
    if (edgeStepsBack >= 0.0)
    {
        /* Work out where the base point would be. */
        vec3 cubeCoordBase = makeCubeCoordFromESB(edgeStepsBack, 0);
        vec4 positionBase = makePositionFromCubeCoord(ClipTransform, cubeCoordBase, basePointEdgeCoord, inData[0].instanceId);
        vec2 screenXYBase = clipToScreenXY(positionBase);

        float adjDist = 1.0; /* always draw _something_ */
        for (int adj = 1; adj <= 3; ++adj)
        {
            vec2 adjPointEdgeCoord = makeEdgeJigsawCoordNearest(edgeJigsawPieceCoord, inData[adj].texCoord + texCoordDisp);
            vec3 cubeCoordAdj = makeCubeCoordFromESB(edgeStepsBack, adj);
            vec4 positionAdj = makePositionFromCubeCoord(ClipTransform, cubeCoordAdj, adjPointEdgeCoord, inData[0].instanceId);
            if (withinView(positionAdj)) /* very important, clipped points will distort the result massively */
            {
                vec2 screenXYAdj = clipToScreenXY(positionAdj);
                adjDist = max(adjDist, distance(screenXYAdj, screenXYBase));
            }
        }

        gl_PointSize = adjDist;
        emitShapeVertexAtPosition(positionBase, cubeCoordBase, WorldTransform, vapourJigsawPieceCoord);
        EndPrimitive();
    }
}

