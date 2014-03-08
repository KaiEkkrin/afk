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

layout (lines_adjacency) in;
layout (points) out;
layout (max_vertices = 1) out;

in VertexData
{
    vec4 feature;
    int instanceId;
} inData[];

out GeometryData
{
    vec3 colour;
    vec3 normal;
} outData;

// I'm going to need to read this again to get the world transform so
// I can calculate a normal
uniform samplerBuffer DisplayTBO;

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

    if (edge && withinView(gl_in[0].gl_Position))
    {
        gl_Position = gl_in[0].gl_Position;

        outData.colour = inData[0].feature.xyz;

        vec4 WTRow1 = texelFetch(DisplayTBO, inData[0].instanceId * 6);
        vec4 WTRow2 = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 1);
        vec4 WTRow3 = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 2);
        vec4 WTRow4 = texelFetch(DisplayTBO, inData[0].instanceId * 6 + 3);
        
        mat4 WorldTransform = mat4(
            vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
            vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
            vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
            vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

        /* Remember, (1, 2, 3) are (x, y, z).
         * Create a normal by biasing each axis by the amount of edge swing involved
         */
        vec4 normal = vec4(
            inData[0].feature.w - inData[1].feature.w,
            inData[0].feature.w - inData[2].feature.w,
            inData[0].feature.w - inData[3].feature.w,
            1.0);
        outData.normal = (WorldTransform * normal).xyz;

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

