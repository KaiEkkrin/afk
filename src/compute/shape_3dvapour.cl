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

/* This is the common part of the vapour stuff, declaring the
 * input units, etc.
 */

struct AFK_3DVapourComputeUnit
{
    float4 location;
    float4 baseColour;
    int4 vapourPiece;
    int adjacency; /* TODO: Once thought I'd want this but now unused -- needs removing. */
    int cubeOffset;
    int cubeCount;
};

__constant sampler_t vapourSampler = CLK_NORMALIZED_COORDS_FALSE |
    CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

float4 readVapourPoint(
    __read_only AFK_IMAGE3D vapourFeature0,
    __read_only AFK_IMAGE3D vapourFeature1,
    __read_only AFK_IMAGE3D vapourFeature2,
    __read_only AFK_IMAGE3D vapourFeature3,
    const int2 fake3D_size,
    const int fake3D_mult,
    __global const struct AFK_3DVapourComputeUnit *units,
    int unitOffset,
    int4 pieceCoord)
{
    int4 myVapourPiece = units[unitOffset].vapourPiece;
    switch (myVapourPiece.w) /* This identifies the jigsaw */
    {
    case 0:
        return read_imagef(vapourFeature0, vapourSampler, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult));

    case 1:
        return read_imagef(vapourFeature1, vapourSampler, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult));

    case 2:
        return read_imagef(vapourFeature2, vapourSampler, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult));

    case 3:
        return read_imagef(vapourFeature3, vapourSampler, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult));

    default:
        /* This really oughtn't to happen, of course... */
        return (float4)(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

void writeVapourPoint(
    __write_only AFK_IMAGE3D vapourNormal0,
    __write_only AFK_IMAGE3D vapourNormal1,
    __write_only AFK_IMAGE3D vapourNormal2,
    __write_only AFK_IMAGE3D vapourNormal3,
    const int2 fake3D_size,
    const int fake3D_mult,
    __global const struct AFK_3DVapourComputeUnit *units,
    int unitOffset,
    int4 pieceCoord,
    float4 normal)
{
    int4 myVapourPiece = units[unitOffset].vapourPiece;
    switch (myVapourPiece.w)
    {
    case 0:
        write_imagef(vapourNormal0, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult), normal);
        break;

    case 1:
        write_imagef(vapourNormal1, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult), normal);
        break;

    case 2:
        write_imagef(vapourNormal2, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult), normal);
        break;

    default:
        write_imagef(vapourNormal3, afk_make3DJigsawCoord(myVapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult), normal);
        break;
    }
}

