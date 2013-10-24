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

// Basis for the shape geometry shaders, defining ways of accessing
// the edge textures and creating a correct vertex from them, etc.
// Expects 6 invocations (one per face).

#version 400

// This is the density texture.  I'll use the first 3 components
// only, which represent the colour.
// TODO: change the vapour to have separate colour (3-uint8) and
// density (1-float) textures?
uniform sampler3D JigsawDensityTex;

// ...and the normal
uniform sampler3D JigsawNormalTex;

// ...and the edge-steps-back information.
uniform usampler2D JigsawESBTex;

// This is the entity display queue.  There are seven texels
// per instance, which contain:
// - first 4: the 4 rows of the transform matrix for the instance
// - fifth: the location relative to the shape origin
// - sixth: the (s, t, r) vapour jigsaw co-ordinates
// - seventh: the (s, t) edge jigsaw co-ordinates
uniform samplerBuffer DisplayTBO; 

// This is the size of an individual vapour jigsaw piece
// in (s, t, r) co-ordinates.
uniform vec3 VapourJigsawPiecePitch;

// This is the size of an individual edge jigsaw piece
// in (s, t) co-ordinates.
uniform vec2 EdgeJigsawPiecePitch;

uniform mat4 ProjectionTransform;
uniform vec2 WindowSize;


/* Makes an edge jigsaw co-ordinate for linear sampling. */
vec2 makeEdgeJigsawCoordLinear(
    vec2 pieceCoord,
    vec2 texCoord)
{
    return pieceCoord + EdgeJigsawPiecePitch * texCoord;
}

/* Makes an edge jigsaw co-ordinate for nearest-neighbour sampling
 * (adding the correct wiggle)
 */
vec2 makeEdgeJigsawCoordNearest(
    vec2 pieceCoord,
    vec2 texCoord)
{
    return pieceCoord + EdgeJigsawPiecePitch * (texCoord + 0.5f / EDIM);
}

/* Makes a vapour jigsaw co-ordinate. */
vec3 makeVapourJigsawCoord(
    vec3 pieceCoord,
    vec3 cubeCoord)
{
    /* It maps to the range 1..(TDIM-1) on the vapour,
     * due to the overlaps in the vapour image:
     * (note the use of the +0.5 "wiggle" here too: it does seem
     * to stop even the linear sampler from running into the gutters,
     * which surprises me)
     */
    return (pieceCoord + VapourJigsawPiecePitch *
        ((cubeCoord * EDIM + 1.5) / TDIM));
}

vec3 makeCubeCoordFromESB(vec2 texCoord, float edgeStepsBack, int face)
{
    /* Construct the cube co-ordinate for this vertex.  It will be
     * in the range 0..1.
     */
    vec3 cubeCoord;
    switch (face)
    {
    case 0:
        cubeCoord = vec3(
            texCoord.x,
            edgeStepsBack / EDIM,
            texCoord.y);
        break;

    case 1:
        cubeCoord = vec3(
            edgeStepsBack / EDIM,
            texCoord.x,
            texCoord.y);
        break;

    case 2:
        cubeCoord = vec3(
            texCoord.x,
            texCoord.y,
            edgeStepsBack / EDIM);
        break;

    case 3:
        cubeCoord = vec3(
            texCoord.x,
            texCoord.y,
            (VDIM - edgeStepsBack) / EDIM);
        break;

    case 4:
        cubeCoord = vec3(
            (VDIM - edgeStepsBack) / EDIM,
            texCoord.x,
            texCoord.y);
        break;

    default:
        cubeCoord = vec3(
            texCoord.x,
            (VDIM - edgeStepsBack) / EDIM,
            texCoord.y);
        break;
    }

    return cubeCoord;
}

vec4 makePositionFromCubeCoord(
    mat4 ClipTransform,
    vec3 cubeCoord,
    vec3 offset,
    int instanceId)
{
    vec4 dispPositionBase = texelFetch(DisplayTBO, instanceId * 7 + 4);

    /* Subtle: note magic use of `w' part of homogeneous
     * dispPositionBase co-ordinates to allow me to add a 0-1 value for
     * displacement within the cube */
    return ClipTransform * (dispPositionBase + vec4(offset, 0.0) +
        vec4(cubeCoord * EDIM / VDIM, 0.0));
}

// Creates the correct displacement for the edge jigsaw in order
// to access the 3x2 face tiling.
vec2 getTexCoordDisp(int face)
{
    vec2 texCoordDisp = vec2(0.0, 0.0);
    switch (face)
    {
    case 1: texCoordDisp = vec2(1.0, 0.0); break;
    case 2: texCoordDisp = vec2(2.0, 0.0); break;
    case 3: texCoordDisp = vec2(0.0, 1.0); break;
    case 4: texCoordDisp = vec2(1.0, 1.0); break;
    case 5: texCoordDisp = vec2(2.0, 1.0); break;
    }
    return texCoordDisp;
}

// Reconstructs the world transform matrix from the texture buffer.
mat4 getWorldTransform(int instanceId)
{
    vec4 WTRow1 = texelFetch(DisplayTBO, instanceId * 7);
    vec4 WTRow2 = texelFetch(DisplayTBO, instanceId * 7 + 1);
    vec4 WTRow3 = texelFetch(DisplayTBO, instanceId * 7 + 2);
    vec4 WTRow4 = texelFetch(DisplayTBO, instanceId * 7 + 3);

    mat4 WorldTransform = mat4(
        vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
        vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
        vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
        vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

    return WorldTransform;
}

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

