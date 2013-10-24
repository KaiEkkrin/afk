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

/* This program computes a 3D shape by edge detecting a
 * 3D `vapour field' (from 3dvapour) at a particular density,
 * rendering this trace into a displacement
 * texture.  This is the second part of the 3dedge stuff,
 * which takes the output of shape_3d (a 3D texture of
 * colours and object presence numbers) and maps the
 * deformable points of a cube onto it.
 *
 * It requires fake3d.
 */


struct AFK_3DEdgeComputeUnit
{
    float4 location; /* TODO Can I simply propagate this into the GL and
                      * get rid of the displacement texture entirely?  How costly
                      * would that be? */
    int4 vapourPiece; /* Contains 1 texel adjacency on all sides */
    int2 edgePiece; /* Points to a 3x2 grid of face textures */
};

enum AFK_ShapeFace
{
    AFK_SHF_BOTTOM  = 0,
    AFK_SHF_LEFT    = 1,
    AFK_SHF_FRONT   = 2,
    AFK_SHF_BACK    = 3,
    AFK_SHF_RIGHT   = 4,
    AFK_SHF_TOP     = 5
};

/* In this function, the co-ordinates xdim and zdim are face co-ordinates.
 * `stepsBack' is the number of steps in vapour space away from this face's
 * base geometry, towards the other side of the cube.
 */
int4 makeVapourCoord(int face, int xdim, int zdim, int stepsBack)
{
    /* Because of the single step of adjacency in the vapour,
     * each new vapour co-ordinate starts at (1, 1, 1).
     * -1 values to xdim, zdim, and stepsBack are valid.
     */
    int4 coord = (int4)(1, 1, 1, 0);

    switch (face)
    {
    case AFK_SHF_BOTTOM:
        coord += (int4)(
            xdim,
            stepsBack,
            zdim,
            0);
        break;

    case AFK_SHF_LEFT:
        coord += (int4)(
            stepsBack,
            xdim,
            zdim,
            0);
        break;

    case AFK_SHF_FRONT:
        coord += (int4)(
            xdim,
            zdim,
            stepsBack,
            0);
        break;

    case AFK_SHF_BACK:
        coord += (int4)(
            xdim,
            zdim,
            VDIM - stepsBack,
            0);
        break;

    case AFK_SHF_RIGHT:
        coord += (int4)(
            VDIM - stepsBack,
            xdim,
            zdim,
            0);
        break;

    case AFK_SHF_TOP:
        coord += (int4)(
            xdim,
            VDIM - stepsBack,
            zdim,
            0);
        break;
    }

    return coord;
}

__constant sampler_t vapourSampler = CLK_NORMALIZED_COORDS_FALSE |
    CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

float4 readVapourPoint(
    __read_only AFK_IMAGE3D vapour,
    const int2 fake3D_size,
    const int fake3D_mult,
    __global const struct AFK_3DEdgeComputeUnit *units,
    int unitOffset,
    int4 pieceCoord)
{
    return read_imagef(vapour, vapourSampler, afk_make3DJigsawCoord(
        units[unitOffset].vapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult));
}

int2 makeEdgeJigsawCoord(__global const struct AFK_3DEdgeComputeUnit *units, int unitOffset, int face, int xdim, int zdim)
{
    int2 baseCoord = (int2)(
        units[unitOffset].edgePiece.x * EDIM * 3,
        units[unitOffset].edgePiece.y * EDIM * 2);

    switch (face)
    {
    case AFK_SHF_BOTTOM:
        baseCoord += (int2)(xdim, zdim);
        break;

    case AFK_SHF_LEFT:
        baseCoord += (int2)(xdim + EDIM, zdim);
        break;

    case AFK_SHF_FRONT:
        baseCoord += (int2)(xdim + 2 * EDIM, zdim);
        break;

    case AFK_SHF_BACK:
        baseCoord += (int2)(xdim, zdim + EDIM);
        break;

    case AFK_SHF_RIGHT:
        baseCoord += (int2)(xdim + EDIM, zdim + EDIM);
        break;

    case AFK_SHF_TOP:
        baseCoord += (int2)(xdim + 2 * EDIM, zdim + EDIM);
        break;
    }

    return baseCoord;
}


/* This parameter list should be sufficient that I will always be able to
 * address all vapour jigsaws in the same place.  I hope!
 *
 * This shader finds the edges of the vapour, and writes their displacements
 * from the faces to the `edgeStepsBack' texture.
 * It also fills out the edge displacement texture, which gives the cube's
 * displacement relative to the overall shape (kind of noddy, but it's there,
 * and saves space in arrays streamed to the GL every render.)
 */
__kernel void makeShape3DEdge(
    __read_only AFK_IMAGE3D vapour,
    __global const struct AFK_3DEdgeComputeUnit *units,
    const int2 fake3D_size,
    const int fake3D_mult,
    __write_only image2d_t jigsawESB)
{
    const int unitOffset = get_global_id(0);
    const int xdim = get_global_id(1); /* 0..EDIM-1 */
    const int zdim = get_global_id(2); /* 0..EDIM-1 */

    /* Iterate through the possible steps back until I find an edge.
     */
    int edgeStepsBack[6];
    for (int face = 0; face < 6; ++face)
    {
        edgeStepsBack[face] = -1;
    }

#define FAKE_TEST_VAPOUR 0

#if !FAKE_TEST_VAPOUR
    for (int stepsBack = 0; stepsBack < (EDIM-1); ++stepsBack)
    {
        for (int face = 0; face < 6; ++face)
        {
            /* Read the next points to compare with */
            int4 lastVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack - 1);
            float4 lastVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, lastVapourPointCoord);

            int4 thisVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack);
            float4 thisVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, thisVapourPointCoord);

            if (lastVapourPoint.w <= 0.0f && thisVapourPoint.w > 0.0f &&
                edgeStepsBack[face] == -1)
            {
                edgeStepsBack[face] = stepsBack;
            }
        }
    }
#endif

    /* Now I can print out the ESB texture. */
    for (int face = 0; face < 6; ++face)
    {
        int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
        write_imageui(jigsawESB, edgeCoord, (uint4)(edgeStepsBack[face], 0, 0, 0));
    }
}

