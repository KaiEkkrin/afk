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

/* This program runs across the 3 dimensions of the density
 * texture.  It produces the min and max densities at each
 * compute unit.  Run it across the (1<<REDUCE_ORDER)**3
 * dimensions.
 *
 * Of course, it might turn out that I can't run across this
 * large a local space and I need to iterate across some of
 * the dimensions instead of reduce...
 *
 * It requires fake3d and shape_3dvapour.
 */

__kernel void makeShapeDReduce(
    __global const struct AFK_3DVapourComputeUnit *units,
    const int2 fake3D_size,
    const int fake3D_mult,
    __read_only AFK_IMAGE3D vapourFeature0,
    __read_only AFK_IMAGE3D vapourFeature1,
    __read_only AFK_IMAGE3D vapourFeature2,
    __read_only AFK_IMAGE3D vapourFeature3,
    __global float *vapourBounds /* 2 consecutive values per unit: (min, max). */
    )
{
    const int unitOffset = get_global_id(0) / TDIM;
    const int xdim = get_local_id(0);
    const int ydim = get_local_id(1);
    const int zdim = get_local_id(2);

    __local float dMin[1<<REDUCE_ORDER][1<<REDUCE_ORDER][1<<REDUCE_ORDER];
    __local float dMax[1<<REDUCE_ORDER][1<<REDUCE_ORDER][1<<REDUCE_ORDER];

    if (xdim < TDIM && ydim < TDIM && zdim < TDIM)
    {
        int4 vapourPointCoord = (int4)(xdim, ydim, zdim, 0);
        float4 vapourPoint = readVapourPoint(vapourFeature0, vapourFeature1, vapourFeature2, vapourFeature3, fake3D_size, fake3D_mult, units, unitOffset, vapourPointCoord);
        dMin[xdim][ydim][zdim] = vapourPoint.w;
        dMax[xdim][ydim][zdim] = vapourPoint.w;
    }
    else
    {
        dMin[xdim][ydim][zdim] = FLT_MAX;
        dMax[xdim][ydim][zdim] = FLT_MIN;
    }

    /* Reduce across the x-axis... */
    for (int xred = (REDUCE_ORDER - 1); xred >= 0; --xred)
    {
        barrier(CLK_LOCAL_MEM_FENCE);

        if (xdim < (1<<xred))
        {
            /* (xdim, xdim + 1<<xred) reduce to (xdim) */
            if (dMin[xdim+(1<<xred)][ydim][zdim] < dMin[xdim][ydim][zdim])
                dMin[xdim][ydim][zdim] = dMin[xdim+(1<<xred)][ydim][zdim];

            if (dMax[xdim+(1<<xred)][ydim][zdim] > dMax[xdim][ydim][zdim])
                dMax[xdim][ydim][zdim] = dMax[xdim+(1<<xred)][ydim][zdim];
        }
    }

    /* ...and across the y-axis... */
    if (xdim == 0)
    {
        for (int yred = (REDUCE_ORDER - 1); yred >= 0; --yred)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (ydim < (1<<yred))
            {
                /* (ydim, ydim + 1<<yred) reduce to (ydim) */
                if (dMin[0][ydim+(1<<yred)][zdim] < dMin[0][ydim][zdim])
                    dMin[0][ydim][zdim] = dMin[0][ydim+(1<<yred)][zdim];
        
                if (dMax[0][ydim+(1<<yred)][zdim] > dMax[0][ydim][zdim])
                    dMax[0][ydim][zdim] = dMax[0][ydim+(1<<yred)][zdim];
                
            }
        }
    }

    /* ...and finally across the z-axis. */
    if (xdim == 0 && ydim == 0)
    {
        for (int zred = (REDUCE_ORDER - 1); zred >= 0; --zred)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (zdim < (1<<zred))
            {
                /* (zdim, zdim + 1<<zred) reduce to (zdim) */
                if (dMin[0][0][zdim+(1<<zred)] < dMin[0][0][zdim])
                    dMin[0][0][zdim] = dMin[0][0][zdim+(1<<zred)];
        
                if (dMax[0][0][zdim+(1<<zred)] > dMax[0][0][zdim])
                    dMax[0][0][zdim] = dMax[0][0][zdim+(1<<zred)];
                
            }
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);
    if (xdim == 0 && ydim == 0 && zdim == 0)
    {
        vapourBounds[2 * unitOffset] = dMin[0][0][0];
        vapourBounds[2 * unitOffset + 1] = dMax[0][0][0];
    }
}

