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

void main()
{
    /* Reconstruct this instance's jigsaw piece coords.
     */
    vec3 vapourJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 4).xyz;
    vec2 edgeJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 5).xy;
    vec2 texCoordDisp = getTexCoordDisp();

    for (uint layer = 0; layer < LAYERS; ++layer)
    {
        mat4 WorldTransform = getWorldTransform(inData[0].instanceId);
        mat4 ClipTransform = ProjectionTransform * WorldTransform;
        emitShapeVertex(ClipTransform, WorldTransform, vapourJigsawPieceCoord, edgeJigsawPieceCoord, texCoordDisp, layer, 0);
        EndPrimitive();
    }
}

