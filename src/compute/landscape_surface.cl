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

int flatTDimCoord(int2 coord)
{
    return ((coord.x * TDIM) + coord.y);
}

void accTriangleNormal(
    float3 v0, float3 v1, float3 v2,
    int2 i0, int2 i1, int2 i2,
    __local float3 *normal)
{
    float3 crossP = cross(v1 - v0, v2 - v0);

    barrier(CLK_LOCAL_MEM_FENCE);
    normal[flatTDimCoord(i0)] += crossP;

    barrier(CLK_LOCAL_MEM_FENCE);
    normal[flatTDimCoord(i1)] += crossP;

    barrier(CLK_LOCAL_MEM_FENCE);
    normal[flatTDimCoord(i2)] += crossP;
}

struct AFK_TerrainComputeUnit
{
    int tileOffset;
    int tileCount;
    int2 piece;
};

/* This program picks through the notional triangles on a terrain tile
 * accumulating surface normals.
 * Run it across (TDIM-1, TDIM-1) dimensions -- it'll scoop up the
 * top and right by itself.
 */

__kernel void makeLandscapeSurface(
    __global const struct AFK_TerrainComputeUnit *units,
    __read_only image2d_t jigsawYDisp,
    sampler_t yDispSampler,
    __write_only image2d_t jigsawNormal)
{
    const int xdim = get_local_id(0);
    const int zdim = get_local_id(1);
    const int unitOffset = get_global_id(2);

    /* Each workspace accumulates the normals for one tile, here: */
    __local float3 normal[TDIM * TDIM];
    
    /* The input grid is VDIM on a
     * side, containing extra vertices all the way around
     * for the purpose of making smooth triangles and joining
     * the tiles together.
     */

    /* Here are the 4 input vertices that make up
     * those 2 triangles...
     */
    int2 i_r1c1 = (int2)(xdim, zdim);
    int2 i_r2c1 = (int2)(xdim + 1, zdim);
    int2 i_r1c2 = (int2)(xdim, zdim + 1);
    int2 i_r2c2 = (int2)(xdim + 1, zdim + 1);

    int2 jigsawOffset = units[unitOffset].piece * TDIM;

    float3 v_r1c1 = (float3)(
        ((float)(i_r1c1.x + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        read_imagef(jigsawYDisp, yDispSampler, jigsawOffset + i_r1c1).x,
        ((float)(i_r1c1.y + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));

    float3 v_r2c1 = (float3)(
        ((float)(i_r2c1.x + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        read_imagef(jigsawYDisp, yDispSampler, jigsawOffset + i_r2c1).x,
        ((float)(i_r2c1.y + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));

    float3 v_r1c2 = (float3)(
        ((float)(i_r1c2.x + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        read_imagef(jigsawYDisp, yDispSampler, jigsawOffset + i_r1c2).x,
        ((float)(i_r1c2.y + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));

    float3 v_r2c2 = (float3)(
        ((float)(i_r2c2.x + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        read_imagef(jigsawYDisp, yDispSampler, jigsawOffset + i_r2c2).x,
        ((float)(i_r2c2.y + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));

    /* Initialise "my" normal */
    normal[flatTDimCoord(i_r1c1)] = (float3)(0.0f, 0.0f, 0.0f);

    /* If I'm on the +x or the +z side, I may own an extra
     * vertex (two if I'm on a corner).
     */
    if (xdim == TDIM - 2)
    {
        normal[flatTDimCoord(i_r2c1)] = (float3)(0.0f, 0.0f, 0.0f);
    }

    if (zdim == TDIM - 2)
    {
        normal[flatTDimCoord(i_r1c2)] = (float3)(0.0f, 0.0f, 0.0f);
    }

    if (xdim == TDIM - 2 && zdim == TDIM - 2)
    {
        normal[flatTDimCoord(i_r2c2)] = (float3)(0.0f, 0.0f, 0.0f);
    }

    accTriangleNormal(
        v_r1c1, v_r1c2, v_r2c1, i_r1c1, i_r1c2, i_r2c1, normal);

    accTriangleNormal(
        v_r1c2, v_r2c2, v_r2c1, i_r1c2, i_r2c2, i_r2c1, normal);

    barrier(CLK_LOCAL_MEM_FENCE);

    /* `normal' now contains nicely accumulated normals that I can
     * write out to the normal jigsaw.
     */
    write_imagef(jigsawNormal, jigsawOffset + i_r1c1, (float4)(normalize(normal[flatTDimCoord(i_r1c1)]), 0.0f));
    if (xdim == TDIM - 2)
    {
        write_imagef(jigsawNormal, jigsawOffset + i_r2c1, (float4)(normalize(normal[flatTDimCoord(i_r2c1)]), 0.0f));
    }

    if (zdim == TDIM - 2)
    {
        write_imagef(jigsawNormal, jigsawOffset + i_r1c2, (float4)(normalize(normal[flatTDimCoord(i_r1c2)]), 0.0f));
    }

    if (xdim == TDIM - 2 && zdim == TDIM - 2)
    {
        write_imagef(jigsawNormal, jigsawOffset + i_r2c2, (float4)(normalize(normal[flatTDimCoord(i_r2c2)]), 0.0f));
    }
}

