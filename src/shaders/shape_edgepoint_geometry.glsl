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

// Shape geometry shader for rendering as points.
// Depends on shape_edgepoint_base.

layout (lines_adjacency) in;
layout (points) out;
layout (max_vertices = 1) out;

in VertexData
{
    vec3 vapourJigsawPieceCoord;
    vec3 cubeCoord;
    vec4 position;
    vec2 screenXY;
    int instanceId;
    float edgeStepsBack;
} inData[];

out GeometryData
{
    vec3 colour;
    vec3 normal;
} outData;

void main()
{
    /* I'll only draw point 0.  However, I'm going to use the other ones
     * to work out a good size to make it:
     */
    if (inData[0].edgeStepsBack >= 0.0)
    {
        float adjDist = 1.0; /* always draw _something_ */
        for (int adj = 1; adj <= 3; ++adj)
        {
            if (inData[adj].edgeStepsBack >= 0.0 &&
                withinView(inData[adj].position)) /* very important, clipped points would distort the result massively */
            {
                adjDist = max(adjDist, distance(inData[adj].screenXY, inData[0].screenXY));
            }
        }
        
        gl_Position = inData[0].position;
        gl_PointSize = adjDist;
        
        mat4 WorldTransform = getWorldTransform(inData[0].instanceId);
        vec3 vapourJigsawPieceCoord = texelFetch(DisplayTBO, inData[0].instanceId * 7 + 5).xyz;
        vec3 vapourJigsawCoord = makeVapourJigsawCoord(vapourJigsawPieceCoord, inData[0].cubeCoord);
        outData.colour = textureLod(JigsawDensityTex, vapourJigsawCoord, 0).xyz;
        vec3 rawNormal = textureLod(JigsawNormalTex, vapourJigsawCoord, 0).xyz;
        outData.normal = mat3(WorldTransform) * rawNormal;
        EmitVertex();

        EndPrimitive();
    }
}

