/* AFK (c) Alex Holloway 2013 */

struct AFK_TerrainComputeUnit
{
    int tileOffset;
    int tileCount;
    int2 piece;
};

/* This program reduces out the y-bounds.
 * Run it across the (1<<REDUCE_ORDER) dimension -- it only
 * reduces in one dimension, because the tile is likely too
 * big to reduce in both.
 */

__kernel void makeLandscapeYReduce(
    __global const struct AFK_TerrainComputeUnit *units,
    __read_only image2d_t jigsawYDisp,
    sampler_t yDispSampler,
    __global float *yBounds /* 2 consecutive values per unit: (lower, upper) */
    )
{
    const int zdim = get_local_id(0);
    const int unitOffset = get_global_id(1);

    __local float yLower[1<<REDUCE_ORDER];
    __local float yUpper[1<<REDUCE_ORDER];

    yLower[zdim] = FLT_MAX;
    yUpper[zdim] = -FLT_MAX;

    /* Iterate across the x axis. */
    int2 jigsawOffset = units[unitOffset].piece * TDIM;
    if (zdim < TDIM)
    {
        for (int xdim = 0; xdim < TDIM; ++xdim)
        {
            float xY = read_imagef(jigsawYDisp, yDispSampler, jigsawOffset + (int2)(xdim, zdim)).x;
            if (xY < yLower[zdim]) yLower[zdim] = xY;
            if (xY > yUpper[zdim]) yUpper[zdim] = xY;
        }
    }

    /* Now, reduce across the z axis. */
    for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
    {
        barrier(CLK_LOCAL_MEM_FENCE);

        if (zdim < (1 << red))
        {
            /* I'm responsible for
             * - zdim
             * - zdim + 1<<red
             * And I'm going to reduce them to
             * - zdim
             */
            int z1 = zdim;
            int z2 = zdim + (1 << red);

            if (yLower[z2] < yLower[z1]) yLower[z1] = yLower[z2];
            if (yUpper[z2] > yUpper[z1]) yUpper[z1] = yUpper[z2];
        }
    }

    /* The zdim=0 threads only are responsible for writing back the results. */
    barrier(CLK_LOCAL_MEM_FENCE);
    if (zdim == 0)
    {
        yBounds[2 * unitOffset] = yLower[0];
        yBounds[2 * unitOffset + 1] = yUpper[0];
    }
}

