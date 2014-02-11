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

// Vertex shader for distant shapes.

#version 400

layout (location = 0) in vec3 Unused;

uniform mat4 ProjectionTransform;

// The distant shape display queue is a list of:
// - world transform (first 16 floats)
// - cell coord (4 floats)
// - colour (4 floats).
uniform samplerBuffer DisplayTBO;

out VertexData
{
    vec4 adjacent[3];
    vec3 colour;
    vec3 normal;
} outData;

void main()
{
    vec4 WTRow1 = texelFetch(DisplayTBO, gl_InstanceID * 6);
    vec4 WTRow2 = texelFetch(DisplayTBO, gl_InstanceID * 6 + 1);
    vec4 WTRow3 = texelFetch(DisplayTBO, gl_InstanceID * 6 + 2);
    vec4 WTRow4 = texelFetch(DisplayTBO, gl_InstanceID * 6 + 3);

    mat4 WorldTransform = mat4(
        vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
        vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
        vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
        vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

    vec4 cellCoord = texelFetch(DisplayTBO, gl_InstanceID * 6 + 4);
    outData.colour = texelFetch(DisplayTBO, gl_InstanceID * 6 + 5).xyz;

    mat4 ClipTransform = ProjectionTransform * WorldTransform;

    // I want to draw my point always in the middle of the cell:
    gl_Position = ClipTransform * (cellCoord + vec4(0.5, 0.5, 0.5, 0.0));

    // I'm going to prepare three adjacent points so that the geometry
    // shader can sort out the point size:
    outData.adjacent[0] = ClipTransform * (cellCoord + vec4(1.5, 0.5, 0.5, 0.0));
    outData.adjacent[1] = ClipTransform * (cellCoord + vec4(0.5, 1.5, 0.5, 0.0));
    outData.adjacent[2] = ClipTransform * (cellCoord + vec4(0.5, 0.5, 1.5, 0.0));

    // I'm going to have the normal always pointing towards the
    // viewer:
    vec4 wCoord = WorldTransform * cellCoord;
    vec3 wCoord3 = vec3(wCoord.x / wCoord.w, wCoord.y / wCoord.w, wCoord.z / wCoord.w);
    outData.normal = -wCoord3;
}

