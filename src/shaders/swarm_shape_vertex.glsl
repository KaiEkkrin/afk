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

// Vertex shader for swarm shapes.
// This vertex shader's business is really just to read the feature
// texture so that the geometry can decide what to do with it.

#version 400

layout (location = 0) in vec3 TexCoord;

uniform mat4 ProjectionTransform;

// This is the vapour feature texture.
uniform sampler3D JigsawFeatureTex;

// TODO: There shall be no separate vapour normal texture.  I should
// throw that away and compute it as I go here, because I'm going to
// need to sample all those places in order to decide whether I'm at
// an edge anyway (no edge == throw the point away).

// The swarm shape display queue is a list of:
// - world transform (first 16 floats)
// - cell coord (4 floats)
// - jigsaw piece s-t-r (4 floats).
uniform samplerBuffer DisplayTBO;

// This is the size of an individual jigsaw piece
// in (s, t, r) co-ordinates.
uniform vec3 JigsawPiecePitch;

out VertexData
{
    vec4 feature; // (r, g, b, density)
    int instanceId;
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
    vec3 jigsawPieceCoord = texelFetch(DisplayTBO, gl_InstanceID * 6 + 5).xyz;

    // The cell coord will be in homogeneous co-ordinates, rigged so
    // that the unit size is the size of one cell
    gl_Position = (ProjectionTransform * WorldTransform) *
        (cellCoord + vec4(TexCoord, 0.0));

    vec3 jigsawCoord = jigsawPieceCoord + (JigsawPiecePitch * TexCoord.xyz);
    outData.feature = textureLod(JigsawFeatureTex, jigsawCoord, 0);
    outData.instanceId = gl_InstanceID;
}

