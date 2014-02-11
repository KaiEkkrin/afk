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

// Geometry shader for swarm shapes.
// This shader inspects a point along with its adjacency, culls points
// that aren't on an edge, and computes clip transform and normal for the
// rest.
// Requires shape_point_size.glsl.

#version 400

layout (lines_adjacency) in;
layout (points) out;
layout (max_vertices = 1) out;

in VertexData
{
    vec4 feature;
} inData[];

out GeometryData
{
    vec3 colour;
    vec3 normal;
} outData;

uniform float EdgeThreshold;

void main()
{
    // Work out whether this point is on an edge.
    bool edge = false;
    if (inData[0].feature.w < EdgeThreshold)
    {
        edge = (inData[1].feature.w > EdgeThreshold ||
            inData[2].feature.w > EdgeThreshold ||
            inData[3].feature.w > EdgeThreshold);
    }
    else
    {
        edge = (inData[1].feature.w < EdgeThreshold ||
            inData[2].feature.w < EdgeThreshold ||
            inData[3].feature.w < EdgeThreshold);
    }

    if (edge && withinView(gl_in[0].gl_Position)
    {
        gl_Position = gl_in[0].gl_Position;

        outData.colour = inData[0].feature.xyz;

        /* Remember, (1, 2, 3) are (x, y, z).
         * Create a normal by biasing each axis by the amount of edge swing involved
         */
        vec3 normal = vec3(
            inData[0].feature.w - inData[1].feature.w,
            inData[0].feature.w - inData[2].feature.w,
            inData[0].feature.w - inData[3].feature.w);
        outData.normal = WorldTransform * normal;

        // Work out how big to make the point by checking how far away the
        // adjacent points would appear:
        vec2 positionXY = clipToScreenXY(gl_Position);
        gl_PointSize = 1.0;
        for (int adj = 1; adj <= 3; ++adj)
        {
            if (withinView(gl_in[adj].gl_Position)) /* very important to avoid distortion */
            {
                vec2 adjPositionXY = clipToScreenXY(gl_in[adj].gl_Position);
                gl_PointSize = ContributeToPointSize(gl_PointSize, positionXY, adjPositionXY);
            }
        }

        EmitVertex();
        EndPrimitive();
    }
}

