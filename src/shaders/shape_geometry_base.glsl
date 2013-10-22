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

// This is the displacement jigsaw texture.
// We sample (x, y, z, w).
uniform sampler2D JigsawDispTex;

// This is the density texture.  I'll use the first 3 components
// only, which represent the colour.
// TODO: change the vapour to have separate colour (3-uint8) and
// density (1-float) textures?
uniform sampler3D JigsawDensityTex;

// ...and the normal
uniform sampler3D JigsawNormalTex;

// ...and the overlap information.
uniform usampler2D JigsawOverlapTex;

// This is the entity display queue.  There are five texels
// per instance, which contain:
// - first 4: the 4 rows of the transform matrix for the instance
// - fifth: (x, y) are the (s, t) jigsaw co-ordinates.
uniform samplerBuffer DisplayTBO; 

// This is the size of an individual vapour jigsaw piece
// in (s, t, r) co-ordinates.
uniform vec3 VapourJigsawPiecePitch;

// This is the size of an individual edge jigsaw piece
// in (s, t) co-ordinates.
uniform vec2 EdgeJigsawPiecePitch;

uniform mat4 ProjectionTransform;
uniform vec2 WindowSize;

in VertexData
{
    int instanceId;
    vec2 texCoord;
} inData[];

out GeometryData
{
    vec3 colour;
    vec3 normal;
} outData;


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

float getEdgeStepsBack(
    vec2 edgeJigsawCoordNN,
    uint layer)
{
    uint esbField = textureLod(JigsawOverlapTex, edgeJigsawCoordNN, 0).y;
    uint esbVal = ((esbField >> (layer * LAYER_BITNESS)) & ((1u<<LAYER_BITNESS)-1)) - 1;
    return float(esbVal);
}

vec3 makeCubeCoordFromESB(float edgeStepsBack, int i)
{
    /* Construct the cube co-ordinate for this vertex.  It will be
     * in the range 0..1.
     */
    vec3 cubeCoord;
    switch (gl_InvocationID)
    {
    case 0:
        cubeCoord = vec3(
            inData[i].texCoord.x,
            edgeStepsBack / EDIM,
            inData[i].texCoord.y);
        break;

    case 1:
        cubeCoord = vec3(
            edgeStepsBack / EDIM,
            inData[i].texCoord.x,
            inData[i].texCoord.y);
        break;

    case 2:
        cubeCoord = vec3(
            inData[i].texCoord.x,
            inData[i].texCoord.y,
            edgeStepsBack / EDIM);
        break;

    case 3:
        cubeCoord = vec3(
            inData[i].texCoord.x,
            inData[i].texCoord.y,
            (VDIM - edgeStepsBack) / EDIM);
        break;

    case 4:
        cubeCoord = vec3(
            (VDIM - edgeStepsBack) / EDIM,
            inData[i].texCoord.x,
            inData[i].texCoord.y);
        break;

    default:
        cubeCoord = vec3(
            inData[i].texCoord.x,
            (VDIM - edgeStepsBack) / EDIM,
            inData[i].texCoord.y);
        break;
    }

    return cubeCoord;
}

vec4 makePositionFromCubeCoord(
    mat4 ClipTransform,
    vec3 cubeCoord,
    vec2 edgeJigsawCoordNN)
{

    /* TODO Right now, I'm writing the same displacement position
     * for every vertex in the same piece in the disp tex -- that's
     * thoroughly suboptimal.  I should change the displacement
     * texture to have just a single texel per piece: but in order
     * to do that I need to support differing piece sizes in the
     * jigsaw (not there yet).
     */
    vec4 dispPositionBase = textureLod(JigsawDispTex, edgeJigsawCoordNN, 0);

    /* Subtle: note magic use of `w' part of homogeneous
     * dispPositionBase co-ordinates to allow me to add a 0-1 value for
     * displacement within the cube */
    return ClipTransform * (dispPositionBase +
        vec4(cubeCoord * EDIM / VDIM, 0.0));
}

void emitShapeVertexAtPosition(
    vec4 position,
    vec3 cubeCoord,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    uint layer)
{
    gl_Position = position;

    /* It maps to the range 1..(TDIM-1) on the vapour,
     * due to the overlaps in the vapour image:
     * (note the use of the +0.5 "wiggle" here too: it does seem
     * to stop even the linear sampler from running into the gutters,
     * which surprises me)
     */
    vec3 vapourJigsawCoord = vapourJigsawPieceCoord + VapourJigsawPiecePitch *
        ((cubeCoord * EDIM + 1.5) / TDIM);

    outData.colour = textureLod(JigsawDensityTex, vapourJigsawCoord, 0).xyz;
    // TODO: Debug colour based on layer, to see what's going on
    //switch (layer)
    //{
    //case 0: outData.colour = vec3(1.0, 0.0, 0.0); break;
    //case 1: outData.colour = vec3(0.0, 1.0, 0.0); break;
    //case 2: outData.colour = vec3(0.0, 0.0, 1.0); break;
    //case 3: outData.colour = vec3(0.0, 1.0, 1.0); break;
    //case 4: outData.colour = vec3(1.0, 0.0, 1.0); break;
    //case 5: outData.colour = vec3(1.0, 1.0, 0.0); break;
    //default: outData.colour = vec3(1.0, 1.0, 1.0); break;
    //}
    vec4 normal = textureLod(JigsawNormalTex, vapourJigsawCoord, 0);
    outData.normal = mat3(WorldTransform) * normal.xyz;
    EmitVertex();
}

void emitShapeVertex(
    mat4 ClipTransform,
    mat4 WorldTransform,
    vec3 vapourJigsawPieceCoord,
    vec2 edgeJigsawPieceCoord,
    vec2 texCoordDisp,
    uint layer,
    int i)
{
    /* This version with the `sample wiggle' necessary to correctly
     * do nearest-neighbour sampling.
     */
    vec2 edgeJigsawCoordNN = makeEdgeJigsawCoordNearest(edgeJigsawPieceCoord, inData[i].texCoord + texCoordDisp);

    float edgeStepsBack = getEdgeStepsBack(edgeJigsawCoordNN, layer);
    vec3 cubeCoord = makeCubeCoordFromESB(edgeStepsBack, i);
    vec4 position = makePositionFromCubeCoord(ClipTransform, cubeCoord, edgeJigsawCoordNN);
    emitShapeVertexAtPosition(position, cubeCoord, WorldTransform, vapourJigsawPieceCoord, layer);
}

// Creates the correct displacement for the edge jigsaw in order
// to access the 3x2 face tiling.
vec2 getTexCoordDisp()
{
    vec2 texCoordDisp = vec2(0.0, 0.0);
    switch (gl_InvocationID)
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
    vec4 WTRow1 = texelFetch(DisplayTBO, instanceId * 6);
    vec4 WTRow2 = texelFetch(DisplayTBO, instanceId * 6 + 1);
    vec4 WTRow3 = texelFetch(DisplayTBO, instanceId * 6 + 2);
    vec4 WTRow4 = texelFetch(DisplayTBO, instanceId * 6 + 3);

    mat4 WorldTransform = mat4(
        vec4(WTRow1.x, WTRow2.x, WTRow3.x, WTRow4.x),
        vec4(WTRow1.y, WTRow2.y, WTRow3.y, WTRow4.y),
        vec4(WTRow1.z, WTRow2.z, WTRow3.z, WTRow4.z),
        vec4(WTRow1.w, WTRow2.w, WTRow3.w, WTRow4.w));

    return WorldTransform;
}

