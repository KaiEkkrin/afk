/* AFK (c) Alex Holloway 2013 */

/* This program computes a 3D shape by edge detecting a
 * 3D `vapour field' (from 3dvapour) at a particular density,
 * rendering this trace into a displacement
 * texture.  This is the second part of the 3dedge stuff,
 * which takes the output of shape_3d (a 3D texture of
 * colours and object presence numbers) and maps the
 * deformable faces of a cube onto it.
 */

/* TODO: For now the comments are all about planning
 * the design of this compute kernel, which is a bit more
 * complicated than the ones I've made before.
 */

/* This kernel should run across:
 * (0..TDIM-1) * unitCount,
 * (0..TDIM-1),
 * (0..TDIM-1).
 */

struct AFK_3DComputeUnit
{
    float4 location;
    int3 vapourPiece;
    int2 edgePiece; /* TODO: Points to the first of six pieces in a row together
                     * along the x axis of the edge jigsaws.  I need to change the
                     * jigsaw so that it can handle allocating pieces in a row
                     * like this. */
    int cubeOffset;
    int cubeCount;
};

__constant int reduceDim = (1 << REDUCE_ORDER);
__constant int halfReduceDim = (1 << (REDUCE_ORDER - 1));

/* This function initialises the local edge array used to reduce
 * faces with -1 (which means "no edge here") in all the places that
 * aren't guaranteed to be overwritten.
 */
void initEdge(__local byte ***edge, int xdim, int ydim, int zdim)
{
    if (xdim < halfReduceDim && ydim < halfReduceDim && zdim < halfReduceDim)
        edge[xdim + halfReduceDim][ydim + halfReduceDim][zdim + halfReduceDim] = -1;
    if (xdim < halfReduceDim && ydim < halfReduceDim)
        edge[xdim + halfReduceDim][ydim + halfReduceDim][zdim] = -1;
    if (xdim < halfReduceDim && zdim < halfReduceDim)
        edge[xdim + halfReduceDim][ydim][zdim + halfReduceDim] = -1;
    if (ydim < halfReduceDim && zdim < halfReduceDim)
        edge[xdim][ydim + halfReduceDim][zdim + halfReduceDim] = -1;
    if (xdim < halfReduceDim)
        edge[xdim + halfReduceDim][ydim][zdim] = -1;
    if (ydim < halfReduceDim)
        edge[xdim][ydim + halfReduceDim][zdim] = -1;
    if (zdim < halfReduceDim)
        edge[xdim][ydim][zdim + halfReduceDim] = -1;

    barrier(CLK_LOCAL_MEM_FENCE);
}

__kernel void makeShape3Dedge(
    __read_only image3d_t vapour,
    sampler_t vapourSampler,
    __global const struct AFK_3DComputeUnit *units,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour,
    __write_only image2d_t jigsawNormal,
    float threshold)
{
    /* We're necessarily going to operate across the
     * three dimensions of a cube.
     * The first dimension should be multiplied up by
     * the unit offset, like so.
     */
    const int unitOffset = get_global_id(0) / TDIM;
    const int xdim = get_global_id(0) % TDIM;
    const int ydim = get_global_id(1);
    const int zdim = get_global_id(2);

    /* Edge-detect by comparing my point in the
     * number grid with the other three points to its
     * top, right and back.
     * (The top and right sides are exempt from this.)
     * This comparison should give me one or more faces
     * complete with information about which direction
     * they are facing in.
     */

    /* Here are the possibilities in 2D:
     * - -                      - -
     * + -  <-- top or right    + +  <-- top only
     *
     * + -                      + -
     * + -  <-- right only      + +  <-- neither; responsibility of the threads +up and +right
     *
     * - +
     * + -  <-- top or right; there may be some illogicality as that + gets handled
     * Invert the above for bottom, left.
     */

    /* This array is (me, right, up, back). */
    float4 v[4];
    v[0] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int3)(xdim, ydim, zdim));
    v[1] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int3)(xdim + 1, ydim, zdim));
    v[2] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int3)(xdim, ydim + 1, zdim));
    v[3] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int3)(xdim, ydim, zdim + 1));

    /* This array flags, for (right, up, back) whether we're a
     * bottom/left/front edge (-1), not an edge (0) or a top/right/back edge
     * (1).
     */
    int edgeFlags[3];
    for (int dir = 0; dir < 3; ++dir)
    {
        if (v[0] <= threshold)
        {
            if (v[dir+1] > threshold) edgeFlags[dir] = -1;
            else edgeFlags[dir] = 0;
        }
        else
        {
            if (v[dir+1] <= threshold) edgeFlags[dir] = 1;
            else edgeFlags[dir] = 0;
        }
    }

    /* Now, I'm going to do each of the six faces in turn.
     * Yes, this is copy-and-paste code.  :(  I can't help it.
     * Any attempt to crunch it together makes a terrible mess,
     * I'd need functors and all sorts.
     * The reduce reduces (dim, dim + 1<<red) into dim on
     * each iteration.
     */

    bool chosen;

    /* --- BOTTOM FACE --- */
    chosen = false;

    {
        __local byte edge[reduceDim][reduceDim][reduceDim];
        initEdge(edge, xdim, ydim, zdim);
        if (edgeFlags[1] == -1) edge[xdim][ydim][zdim] = ydim;

        for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (ydim < (1 << red))
            {
                if (edge[xdim][ydim][zdim] == -1 &&
                    edge[xdim][ydim + 1<<red][zdim] != -1)
                {
                    edge[xdim][ydim][zdim] = edge[xdim][ydim + 1<<red][zdim];
                }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (ydim == edge[xdim][0][zdim]) chosen = true;
    }

    if (chosen)
    {
        /* I have been chosen.  TODO The normal calculation goes here: I
         * need to identify the co-ordinates that contribute and accumulate
         * it into a local face of normals.  But I'm not sure how to
         * do that yet, I'll get the displacement and colour working first
         * without normals.
         */
        int2 jigsawCoord = edgePiece + (int2)(xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            (float)ydim / (float)POINT_SUBDIVISION_FACTOR);
        write_imagef(jigsawColour, jigsawCoord, v.xyz);
        edgeFlags[1] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- LEFT FACE --- */
    chosen = false;

    {
        __local byte edge[reduceDim][reduceDim][reduceDim];
        initEdge(edge, xdim, ydim, zdim);
        if (edgeFlags[0] == -1) edge[xdim][ydim][zdim] = xdim;

        for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (xdim < (1 << red))
            {
                if (edge[xdim][ydim][zdim] == -1 &&
                    edge[xdim + 1<<red][ydim][zdim] != -1)
                {
                    edge[xdim][ydim][zdim] = edge[xdim + 1<<red][ydim][zdim];
                }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (xdim == edge[0][ydim][zdim]) chosen = true;
    }

    if (chosen)
    {
        int2 jigsawCoord = edgePiece + (int2)(TDIM + xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            (float)ydim / (float)POINT_SUBDIVISION_FACTOR);
        write_imagef(jigsawColour, jigsawCoord, v.xyz);

        /* We used that edge, so reset its flag so it doesn't get
         * overlapped in another face.
         * Otherwise, we can re-try.
         */
        edgeFlags[0] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- FRONT FACE --- */
    chosen = false;

    {
        __local byte edge[reduceDim][reduceDim][reduceDim];
        initEdge(edge, xdim, ydim, zdim);
        if (edgeFlags[2] == -1) edge[xdim][ydim][zdim] = zdim;

        for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (zdim < (1 << red))
            {
                if (edge[xdim][ydim][zdim] == -1 &&
                    edge[xdim][ydim][zdim + 1<<red] != -1)
                {
                    edge[xdim][ydim][zdim] = edge[xdim][ydim][zdim + 1<<red];
                }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (zdim == edge[xdim][ydim][0]) chosen = true;
    }

    if (chosen)
    {
        int2 jigsawCoord = edgePiece + (int2)(2 * TDIM + xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            (float)ydim / (float)POINT_SUBDIVISION_FACTOR);
        write_imagef(jigsawColour, jigsawCoord, v.xyz);
        edgeFlags[2] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- BACK FACE --- */
    chosen = false;

    {
        __local byte edge[reduceDim][reduceDim][reduceDim];
        initEdge(edge, xdim, ydim, zdim);
        if (edgeFlags[2] == 1) edge[xdim][ydim][zdim] = zdim;

        for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (zdim < (1 << red))
            {
                if (edge[xdim][ydim][zdim + 1<<red] != -1)
                {
                    edge[xdim][ydim][zdim] = edge[xdim][ydim][zdim + 1<<red];
                }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (zdim == edge[xdim][ydim][0]) chosen = true;
    }

    if (chosen)
    {
        int2 jigsawCoord = edgePiece + (int2)(xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            (float)ydim / (float)POINT_SUBDIVISION_FACTOR);
        write_imagef(jigsawColour, jigsawCoord, v.xyz);
        edgeFlags[2] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- RIGHT FACE --- */
    chosen = false;

    {
        __local byte edge[reduceDim][reduceDim][reduceDim];
        initEdge(edge, xdim, ydim, zdim);
        if (edgeFlags[0] == 1) edge[xdim][ydim][zdim] = xdim;

        for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (xdim < (1 << red))
            {
                if (edge[xdim + 1<<red][ydim][zdim] != -1)
                {
                    edge[xdim][ydim][zdim] = edge[xdim + 1<<red][ydim][zdim];
                }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (xdim == edge[0][ydim][zdim]) chosen = true;
    }

    if (chosen)
    {
        int2 jigsawCoord = edgePiece + (int2)(TDIM + xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            (float)ydim / (float)POINT_SUBDIVISION_FACTOR);
        write_imagef(jigsawColour, jigsawCoord, v.xyz);
        edgeFlags[0] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- TOP FACE --- */
    chosen = false;

    {
        __local byte edge[reduceDim][reduceDim][reduceDim];
        initEdge(edge, xdim, ydim, zdim);
        if (edgeFlags[1] == 1) edge[xdim][ydim][zdim] = ydim;

        for (int red = (REDUCE_ORDER - 1); red >= 0; --red)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            if (ydim < (1 << red))
            {
                if (edge[xdim][ydim + 1<<red][zdim] != -1)
                {
                    edge[xdim][ydim][zdim] = edge[xdim][ydim + 1<<red][zdim];
                }
            }
        }

        barrier(CLK_LOCAL_MEM_FENCE);

        if (ydim == edge[xdim][0][zdim]) chosen = true;
    }

    if (chosen)
    {
        int2 jigsawCoord = edgePiece + (int2)(2 * TDIM + xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            (float)ydim / (float)POINT_SUBDIVISION_FACTOR);
        write_imagef(jigsawColour, jigsawCoord, v.xyz);
        edgeFlags[1] = 0;
    }

    /* Now: `surface' isn't going to work for these
     * shapes, not unless I put lots of overlap around all
     * the edges (fail).  However, the number field gives
     * me lots of information to make lovely normals,
     * by using the numbers themselves to compute a more
     * detailed slant.  Consider how I'd implement this.
     */
}

