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

/* This program runs across 2 dimensions of the density
 * texture.  It produces the min and max densities at each
 * compute unit.  Run it across:
 * - (unit count, 1<<REDUCE_ORDER, 1<<REDUCE_ORDER) globally,
 * - (1, 1<<REDUCE_ORDER, 1<<REDUCE_ORDER) locally.
 * (a 3-dimensional reduce would result in an overly large
 * work size.)
 *
 * Of course, it might turn out that I can't run across this
 * large a local space and I need to iterate across some of
 * the dimensions instead of reduce...
 *
 * It requires fake3d and shape_3dvapour.
 */

__kernel void makeShape3DVapourDReduce(
    __global const struct AFK_3DVapourComputeUnit *units,
    const int2 fake3D_size,
    const int fake3D_mult,
    __read_only AFK_IMAGE3D vapourFeature0,
    __read_only AFK_IMAGE3D vapourFeature1,
    __read_only AFK_IMAGE3D vapourFeature2,
    __read_only AFK_IMAGE3D vapourFeature3,
    __global float2 *vapourBounds, /* min, max */
    __global uchar4 *colourAvg /* red, green, blue, unused */
    )
{
    const int unitOffset = get_global_id(0);
    const int ydim = get_local_id(1);
    const int zdim = get_local_id(2);

    __local float dMin[1<<REDUCE_ORDER][1<<REDUCE_ORDER];
    __local float dMax[1<<REDUCE_ORDER][1<<REDUCE_ORDER];
    __local float4 col[1<<REDUCE_ORDER][1<<REDUCE_ORDER];

    /* Initialise those (y, z) arrays, and iterate across the
     * x dimensions where relevant
     */
    dMin[ydim][zdim] = FLT_MAX;
    dMax[ydim][zdim] = FLT_MIN;
    col[ydim][zdim] = (float4)(0.0f, 0.0f, 0.0f, 0.0f);

    if (ydim < TDIM && zdim < TDIM)
    {
        for (int xdim = 0; xdim < TDIM; ++xdim)
        {
            int4 vapourPointCoord = (int4)(xdim, ydim, zdim, 0);
            float4 vapourPoint = readVapourPoint(vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3, fake3D_size, fake3D_mult, units, unitOffset, vapourPointCoord);

            if (vapourPoint.w < dMin[ydim][zdim]) dMin[ydim][zdim] = vapourPoint.w;
            if (vapourPoint.w > dMax[ydim][zdim]) dMax[ydim][zdim] = vapourPoint.w;

            col[ydim][zdim] += vapourPoint;
        }
    }

    /* Reduce across the y-axis... */
    for (int yred = (REDUCE_ORDER - 1); yred >= 0; --yred)
    {
        barrier(CLK_LOCAL_MEM_FENCE);

        if (ydim < (1<<yred))
        {
            /* (ydim, ydim + 1<<yred) reduce to (ydim) */
            if (dMin[ydim+(1<<yred)][zdim] < dMin[ydim][zdim])
                dMin[ydim][zdim] = dMin[ydim+(1<<yred)][zdim];
    
            if (dMax[ydim+(1<<yred)][zdim] > dMax[ydim][zdim])
                dMax[ydim][zdim] = dMax[ydim+(1<<yred)][zdim];
            
            col[ydim][zdim] += col[ydim+(1<<yred)][zdim];
        }
    }

    /* ...and across the z-axis. */
    for (int zred = (REDUCE_ORDER - 1); zred >= 0; --zred)
    {
        barrier(CLK_LOCAL_MEM_FENCE);

        if (ydim == 0 && zdim < (1<<zred))
        {
            /* (zdim, zdim + 1<<zred) reduce to (zdim) */
            if (dMin[0][zdim+(1<<zred)] < dMin[0][zdim])
                dMin[0][zdim] = dMin[0][zdim+(1<<zred)];
    
            if (dMax[0][zdim+(1<<zred)] > dMax[0][zdim])
                dMax[0][zdim] = dMax[0][zdim+(1<<zred)];
            
            col[0][zdim] += col[0][zdim+(1<<zred)];
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);
    if (ydim == 0 && zdim == 0)
    {
        vapourBounds[unitOffset] = (float2)(dMin[0][0], dMax[0][0]);
        
        float4 colAvg = normalize((float4)(
            col[0][0].x, col[0][0].y, col[0][0].z, 0.0f));
        colourAvg[unitOffset] = (uchar4)(
            (uchar)(colAvg.x * 256.0f),
            (uchar)(colAvg.y * 256.0f),
            (uchar)(colAvg.z * 256.0f),
            0);
    }
}

