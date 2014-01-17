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

/* This kernel creates a 3D vapour normal texture out of the feature
 * texture (made by shape_3dvapour_features).
 *
 * It requires fake3d and shape_3dvapour.
 */

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

/* This function decides whether a coord component is
 * within this work unit's cube.
 */
bool coordWithinCube(int c)
{
    return (c >= 0 && c < TDIM);
}

/* This function tries to calculate a normal around a vapour
 * point and points displaced from it by the given vector.
 * TODO: This function nabbed from shape_3dedge.  After I've
 * got the reading of 3D textures working okay, I can remove
 * it from there (and remove the normal info from the edge
 * textures).
 */
float3 make4PointNormal(
    __read_only AFK_IMAGE3D vapourFeature0,
    __read_only AFK_IMAGE3D vapourFeature1,
    __read_only AFK_IMAGE3D vapourFeature2,
    __read_only AFK_IMAGE3D vapourFeature3,
    const int2 fake3D_size,
    const int fake3D_mult,
    __global const struct AFK_3DVapourComputeUnit *units,
    int unitOffset,
    int4 thisVapourPointCoord,
    int4 displacement) /* (x, y, z) should each be 1 or -1 */
{
    /* Ah, the normal.  Consider two orthogonal axes (x, y):
     * If the difference (left - this) is zero, the normal at `this' is
     * perpendicular to x.
     * As that difference tends to infinity, the normal tends towards parallel
     * to x.
     * The reverse occurs w.r.t. the y axis.
     * If (left - this) == (last - this), the angle between the normal
     * and both x and y axes is 45 degrees.
     * What function describes this effect?
     * Note: tan(x) is the ratio between the 2 shorter sides of the
     * triangle, where x is the angle at the apex.
     * However, I don't really need the angle do I?  Don't I need to normalize
     * the (x, y) vector,
     * (last - this, left - this) ?
     *
     * What's the 3D equivalent, for (this, left, front, last) ?
     * (sqrt((front - this)**2 + (last - this)**2),
     *  sqrt((left - this)**2 + (last - this)**2),
     *  sqrt((left - this)**2 + (front - this)**2)) perhaps ?
     * Worth a try.  (Hmm; those square roots are also distances.  Perhaps I
     * could try to short circuit a plot of this by finding the 3D point,
     * (left, front, last) and taking distance((left, front, last) - (this, this, this)) ?
     * Then normalize and sum the vectors for each of,
     * - (this, left, front, last)
     * - (this, front, right, last)
     * - (this, right, back, last)
     * - (this, back, left, last)
     * Where the `left' and `front' square roots should be subtracted...  I think
     * and rotate it into world space...  (do that after the basic thing looks plausible...)
     */

    int4 xVapourPointCoord = thisVapourPointCoord + (int4)(displacement.x, 0, 0, 0);
    int4 yVapourPointCoord = thisVapourPointCoord - (int4)(0, displacement.y, 0, 0);
    int4 zVapourPointCoord = thisVapourPointCoord - (int4)(0, 0, displacement.z, 0);

    float4 thisVapourPoint = readVapourPoint(vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3, fake3D_size, fake3D_mult, units, unitOffset, thisVapourPointCoord);
    float4 xVapourPoint = readVapourPoint(vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3, fake3D_size, fake3D_mult, units, unitOffset, xVapourPointCoord);
    float4 yVapourPoint = readVapourPoint(vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3, fake3D_size, fake3D_mult, units, unitOffset, yVapourPointCoord);
    float4 zVapourPoint = readVapourPoint(vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3, fake3D_size, fake3D_mult, units, unitOffset, zVapourPointCoord);

    float3 combinedVectors = (float3)(
        (xVapourPoint.w - thisVapourPoint.w) * displacement.x,
        (yVapourPoint.w - thisVapourPoint.w) * displacement.y,
        (zVapourPoint.w - thisVapourPoint.w) * displacement.z);

    return combinedVectors;
}

__kernel void makeShape3DVapourNormal(
    __global const struct AFK_3DVapourComputeUnit *units,
    const int2 fake3D_size,
    const int fake3D_mult,
    __read_only AFK_IMAGE3D vapourFeature0,
    __read_only AFK_IMAGE3D vapourFeature1,
    __read_only AFK_IMAGE3D vapourFeature2,
    __read_only AFK_IMAGE3D vapourFeature3,
    __write_only AFK_IMAGE3D vapourNormal0,
    __write_only AFK_IMAGE3D vapourNormal1,
    __write_only AFK_IMAGE3D vapourNormal2,
    __write_only AFK_IMAGE3D vapourNormal3)
{
    const int unitOffset = get_global_id(0) / TDIM;
    const int xdim = get_global_id(0) % TDIM;
    const int ydim = get_global_id(1);
    const int zdim = get_global_id(2);

    int4 thisVapourPointCoord = (int4)(xdim, ydim, zdim, 0);
    float3 thisNormal = (float3)(0.0f, 0.0f, 0.0f);

    for (int xN = -1; xN <= 1; xN += 2)
    {
        for (int yN = -1; yN <= 1; yN += 2)
        {
            for (int zN = -1; zN <= 1; zN += 2)
            {
                if (coordWithinCube(xdim + xN) &&
                    coordWithinCube(ydim + yN) &&
                    coordWithinCube(zdim + zN))
                {
                    thisNormal += make4PointNormal(
                        vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3,
                        fake3D_size, fake3D_mult,
                        units, unitOffset, thisVapourPointCoord, (int4)(xN, yN, zN, 0));
                }
            }
        }
    }

    writeVapourPoint(vapourNormal0, vapourNormal1, vapourNormal2, vapourNormal3,
        fake3D_size, fake3D_mult,
        units, unitOffset, thisVapourPointCoord,
        (float4)(normalize(thisNormal), 0.0f));
}

