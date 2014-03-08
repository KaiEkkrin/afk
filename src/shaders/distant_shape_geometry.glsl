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

// Geometry shader for distant shapes.
// You might not expect one, but I want to make sure I cull points that
// stray outside the view to avoid distorted point sizes here.
// Requires shape_point_size.glsl.

layout (points) in;
layout (points) out;
layout (max_vertices = 1) out;

in VertexData
{
    vec4 adjacent[3];
    vec3 colour;
    vec3 normal;
} inData[];

out GeometryData
{
    vec3 colour;
    vec3 normal;
} outData;

void main()
{
    if (withinView(gl_in[0].gl_Position))
    {
        gl_Position = gl_in[0].gl_Position;

        outData.colour = inData[0].colour;
        outData.normal = inData[0].normal;

        // Work out how big to make the point by checking how far away points
        // for the three adjacent cubes would be:
        vec2 positionXY = clipToScreenXY(gl_Position);
        gl_PointSize = 1.0;
        for (int adj = 0; adj < 3; ++adj)
        {
            if (withinView(inData[0].adjacent[adj])) /* very important to avoid distortion */
            {
                vec2 adjPositionXY = clipToScreenXY(inData[0].adjacent[adj]);
                gl_PointSize = ContributeToPointSize(gl_PointSize, positionXY, adjPositionXY);
            }
        }

        EmitVertex();
        EndPrimitive();
    }
}

