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
    int4 vapourPiece;
    int2 edgePiece; /* Points to a 3x2 grid of face textures */
    int cubeOffset;
    int cubeCount;
};

/* REDUCE_DIM will be (1 << REDUCE_ORDER). */
#define HALF_REDUCE_DIM (1 << (REDUCE_ORDER - 1))

/* This macro initialises the local edge array used to reduce
 * faces with -1 (which means "no edge here") in all the places that
 * aren't guaranteed to be overwritten.
 */
#if 1
#define INIT_EDGE(edge, xdim, ydim, zdim) \
{ \
    if ((xdim) < HALF_REDUCE_DIM && (ydim) < HALF_REDUCE_DIM && (zdim) < HALF_REDUCE_DIM) \
        (edge)[(xdim) + HALF_REDUCE_DIM][(ydim) + HALF_REDUCE_DIM][(zdim) + HALF_REDUCE_DIM] = -1; \
    if ((xdim) < HALF_REDUCE_DIM && (ydim) < HALF_REDUCE_DIM) \
        (edge)[(xdim) + HALF_REDUCE_DIM][(ydim) + HALF_REDUCE_DIM][(zdim)] = -1; \
    if ((xdim) < HALF_REDUCE_DIM && (zdim) < HALF_REDUCE_DIM) \
        (edge)[(xdim) + HALF_REDUCE_DIM][(ydim)][(zdim) + HALF_REDUCE_DIM] = -1; \
    if ((ydim) < HALF_REDUCE_DIM && (zdim) < HALF_REDUCE_DIM) \
        (edge)[(xdim)][(ydim) + HALF_REDUCE_DIM][(zdim) + HALF_REDUCE_DIM] = -1; \
    if ((xdim) < HALF_REDUCE_DIM) \
        (edge)[(xdim) + HALF_REDUCE_DIM][(ydim)][(zdim)] = -1; \
    if ((ydim) < HALF_REDUCE_DIM) \
        (edge)[(xdim)][(ydim) + HALF_REDUCE_DIM][(zdim)] = -1; \
    if ((zdim) < HALF_REDUCE_DIM) \
        (edge)[(xdim)][(ydim)][(zdim) + HALF_REDUCE_DIM] = -1; \
    barrier(CLK_LOCAL_MEM_FENCE); \
}
#else
#define INIT_EDGE(edge, xdim, ydim, zdim) \
{ \
    barrier(CLK_LOCAL_MEM_FENCE); \
}
#endif

/* This function makes the displacement co-ordinate at a vapour point.
 * I'm making this as a homogeneous co-ordinate because I've necessarily
 * got a 4-vector, and because it theoretically means I could cram my
 * co-ordinates into lower precision numbers without losing so much
 * accuracy.  The shader needs to cope with it.
 */
float4 makeVapourCoord(int xdim, int ydim, int zdim, float4 location)
{
    float3 baseCoord = (float3)(
        ((float)(xdim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        ((float)(ydim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        ((float)(zdim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));
    return (float4)(
        baseCoord + location.xyz,
        location.w);
}

__kernel void makeShape3DEdge(
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

    /* Initialise the image as quickly as I can, because
     * this is going to look a bit nasty.
     * Nodes that don't get identified below need a default
     * value that will stop anything from being drawn.
     * (I'm going to be dropping plenty of vertices).
     */
    if (ydim == (0 % TDIM))
    {
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord, (float4)(NAN, NAN, NAN, NAN));
        write_imagef(jigsawColour, jigsawCoord, (float4)(0.0f, 0.0f, 0.0f, 0.0f));
        write_imagef(jigsawNormal, jigsawCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));
    }

    if (ydim == (1 % TDIM))
    {
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(TDIM + xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord, (float4)(NAN, NAN, NAN, NAN));
        write_imagef(jigsawColour, jigsawCoord, (float4)(0.0f, 0.0f, 0.0f, 0.0f));
        write_imagef(jigsawNormal, jigsawCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));
    }

    if (ydim == (2 % TDIM))
    {
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(TDIM * 2 + xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord, (float4)(NAN, NAN, NAN, NAN));
        write_imagef(jigsawColour, jigsawCoord, (float4)(0.0f, 0.0f, 0.0f, 0.0f));
        write_imagef(jigsawNormal, jigsawCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));
    }

    if (ydim == (3 % TDIM))
    {
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord, (float4)(NAN, NAN, NAN, NAN));
        write_imagef(jigsawColour, jigsawCoord, (float4)(0.0f, 0.0f, 0.0f, 0.0f));
        write_imagef(jigsawNormal, jigsawCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));
    }

    if (ydim == (4 % TDIM))
    {
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(TDIM + xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord, (float4)(NAN, NAN, NAN, NAN));
        write_imagef(jigsawColour, jigsawCoord, (float4)(0.0f, 0.0f, 0.0f, 0.0f));
        write_imagef(jigsawNormal, jigsawCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));
    }

    if (ydim == (5 % TDIM))
    {
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(TDIM * 2 + xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord, (float4)(NAN, NAN, NAN, NAN));
        write_imagef(jigsawColour, jigsawCoord, (float4)(0.0f, 0.0f, 0.0f, 0.0f));
        write_imagef(jigsawNormal, jigsawCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));
    }

    barrier(CLK_GLOBAL_MEM_FENCE);

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
        (int4)(xdim, ydim, zdim, 0));
    v[1] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int4)(xdim + 1, ydim, zdim, 0));
    v[2] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int4)(xdim, ydim + 1, zdim, 0));
    v[3] = read_imagef(vapour, vapourSampler, units[unitOffset].vapourPiece * TDIM +
        (int4)(xdim, ydim, zdim + 1, 0));

    /* This array flags, for (right, up, back) whether we're a
     * bottom/left/front edge (-1), not an edge (0) or a top/right/back edge
     * (1).
     */
    int edgeFlags[3];
    for (int dir = 0; dir < 3; ++dir)
    {
        if (v[0].w <= threshold)
        {
            if (v[dir+1].w > threshold) edgeFlags[dir] = -1;
            else edgeFlags[dir] = 0;
        }
        else
        {
            if (v[dir+1].w <= threshold) edgeFlags[dir] = 1;
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
        __local unsigned char edge[REDUCE_DIM][REDUCE_DIM][REDUCE_DIM];
        INIT_EDGE(edge, xdim, ydim, zdim)
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
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(xdim, zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            makeVapourCoord(xdim, ydim, zdim, units[unitOffset].location));
        write_imagef(jigsawColour, jigsawCoord, v[0]);
        edgeFlags[1] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- LEFT FACE --- */
    chosen = false;

    {
        __local unsigned char edge[REDUCE_DIM][REDUCE_DIM][REDUCE_DIM];
        INIT_EDGE(edge, xdim, ydim, zdim)
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
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(TDIM + ydim, zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            makeVapourCoord(xdim, ydim, zdim, units[unitOffset].location));
        write_imagef(jigsawColour, jigsawCoord, v[0]);

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
        __local unsigned char edge[REDUCE_DIM][REDUCE_DIM][REDUCE_DIM];
        INIT_EDGE(edge, xdim, ydim, zdim)
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
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(2 * TDIM + xdim, ydim);
        write_imagef(jigsawDisp, jigsawCoord,
            makeVapourCoord(xdim, ydim, zdim, units[unitOffset].location));
        write_imagef(jigsawColour, jigsawCoord, v[0]);
        edgeFlags[2] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- BACK FACE --- */
    chosen = false;

    {
        __local unsigned char edge[REDUCE_DIM][REDUCE_DIM][REDUCE_DIM];
        INIT_EDGE(edge, xdim, ydim, zdim)
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
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(xdim, TDIM + ydim);
        write_imagef(jigsawDisp, jigsawCoord,
            makeVapourCoord(xdim, ydim, zdim, units[unitOffset].location));
        write_imagef(jigsawColour, jigsawCoord, v[0]);
        edgeFlags[2] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- RIGHT FACE --- */
    chosen = false;

    {
        __local unsigned char edge[REDUCE_DIM][REDUCE_DIM][REDUCE_DIM];
        INIT_EDGE(edge, xdim, ydim, zdim)
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
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(TDIM + ydim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            makeVapourCoord(xdim, ydim, zdim, units[unitOffset].location));
        write_imagef(jigsawColour, jigsawCoord, v[0]);
        edgeFlags[0] = 0;
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* --- TOP FACE --- */
    chosen = false;

    {
        __local unsigned char edge[REDUCE_DIM][REDUCE_DIM][REDUCE_DIM];
        INIT_EDGE(edge, xdim, ydim, zdim)
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
        int2 jigsawCoord = units[unitOffset].edgePiece.xy + (int2)(2 * TDIM + xdim, TDIM + zdim);
        write_imagef(jigsawDisp, jigsawCoord,
            makeVapourCoord(xdim, ydim, zdim, units[unitOffset].location));
        write_imagef(jigsawColour, jigsawCoord, v[0]);
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

