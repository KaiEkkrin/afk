/* AFK (c) Alex Holloway 2013 */

/* This program computes a 3D shape by edge detecting a
 * 3D `vapour field' (from 3dvapour) at a particular density,
 * rendering this trace into a displacement
 * texture.  This is the second part of the 3dedge stuff,
 * which takes the output of shape_3d (a 3D texture of
 * colours and object presence numbers) and maps the
 * deformable faces of a cube onto it.
 */

/* Abstraction around 3D images to support emulation.
 * TODO Pull out into some kind of library .cl.
 */

#if AFK_FAKE3D

#define AFK_IMAGE3D image2d_t

int2 afk_from3DTo2DCoord(int4 coord)
{
    /* The 3D wibble applies within the tile.
     * The tiles themselves will be supplied in a 2D grid.
     * (Because we're using a 2D jigsaw.)
     */
    return (int2)(
        coord.x + VAPOUR_FAKE3D_FAKESIZE_X * (coord.z % VAPOUR_FAKE3D_MULT),
        coord.y + VAPOUR_FAKE3D_FAKESIZE_Y * (coord.z / VAPOUR_FAKE3D_MULT));
}

int2 afk_make3DJigsawCoord(int4 pieceCoord, int4 pointCoord)
{
    int2 pieceCoord2D = (int2)(
        pieceCoord.x * VAPOUR_FAKE3D_FAKESIZE_X * VAPOUR_FAKE3D_MULT,
        pieceCoord.y * VAPOUR_FAKE3D_FAKESIZE_Y * VAPOUR_FAKE3D_MULT);
    int2 pointCoord2D = afk_from3DTo2DCoord(pointCoord);
    return pieceCoord2D + pointCoord2D;
}

#else
#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

#define AFK_IMAGE3D image3d_t

int4 afk_make3DJigsawCoord(int4 pieceCoord, int4 pointCoord)
{
    return pieceCoord * VDIM + pointCoord;
}

#endif /* AFK_FAKE3D */

struct AFK_3DComputeUnit
{
    float4 location;
    int4 vapourPiece;
    int2 edgePiece; /* Points to a 3x2 grid of face textures */
    int cubeOffset;
    int cubeCount;
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
    int4 coord = (int4)(0, 0, 0, 0);

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

float4 readVapourPoint(__read_only AFK_IMAGE3D vapour, __global const struct AFK_3DComputeUnit *units, int unitOffset, int4 pieceCoord)
{
    /* TODO: To avoid needing vapour piece cross-referencing quite yet,
     * I'm going to clamp all the elements of pieceCoord to VDIM-1 and
     * not stray up to VDIM (next piece)
     */
    pieceCoord.x = min(pieceCoord.x, VDIM-1);
    pieceCoord.y = min(pieceCoord.y, VDIM-1);
    pieceCoord.z = min(pieceCoord.z, VDIM-1);

    return read_imagef(vapour, vapourSampler, afk_make3DJigsawCoord(units[unitOffset].vapourPiece, pieceCoord));
}

int2 makeEdgeJigsawCoord(__global const struct AFK_3DComputeUnit *units, int unitOffset, int face, int xdim, int zdim)
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

/* This function makes the displacement co-ordinate at a vapour point.
 * I'm making this as a homogeneous co-ordinate because I've necessarily
 * got a 4-vector, and because it theoretically means I could cram my
 * co-ordinates into lower precision numbers without losing so much
 * accuracy.  The shader needs to cope with it.
 */
float4 makeEdgeVertex(int face, int xdim, int zdim, int stepsBack, float4 location)
{
    float3 baseVertex;

    switch (face)
    {
    case AFK_SHF_BOTTOM:
        baseVertex = (float3)((float)xdim, (float)stepsBack, (float)zdim);
        break;

    case AFK_SHF_LEFT:
        baseVertex = (float3)((float)stepsBack, (float)xdim, (float)zdim);
        break;

    case AFK_SHF_FRONT:
        baseVertex = (float3)((float)xdim, (float)zdim, (float)stepsBack);
        break;

    case AFK_SHF_BACK:
        baseVertex = (float3)((float)xdim, (float)zdim, (float)(VDIM - stepsBack));
        break;

    case AFK_SHF_RIGHT:
        baseVertex = (float3)((float)(VDIM - stepsBack), (float)xdim, (float)zdim);
        break;

    case AFK_SHF_TOP:
        baseVertex = (float3)((float)xdim, (float)(VDIM - stepsBack), (float)zdim);
        break;
    }

    baseVertex = baseVertex / (float)POINT_SUBDIVISION_FACTOR;
    return (float4)(
        baseVertex + location.xyz,
        location.w);
}

__kernel void makeShape3DEdge(
    __read_only AFK_IMAGE3D vapour,
    __global const struct AFK_3DComputeUnit *units,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour,
    __write_only image2d_t jigsawNormal,
    float threshold)
{
#if 0
    const int unitOffset = get_global_id(0) / 6;
    const int face = get_local_id(0); /* 0..6 */
    const int xdim = get_local_id(1); /* 0..EDIM-1 */
    const int zdim = get_local_id(2); /* 0..EDIM-1 */

    /* Here I track whether each of the vapour points has already been
     * written as an edge.
     * Each word starts at -1 and is updated with which face got the
     * right to draw that edge. 
     */
    __local char pointsDrawn[VDIM][VDIM][VDIM];

    /* Initialize that flag array. */
    for (int y = face; y < VDIM; y += face)
    {
        pointsDrawn[xdim][y][zdim] = -1;
    }
#else
    const int unitOffset = get_global_id(0) / 6;
    const int face = get_global_id(0) % 6; /* 0..6 */
    const int xdim = get_global_id(1); /* 0..EDIM-1 */
    const int zdim = get_global_id(2); /* 0..EDIM-1 */
#endif

    /* Iterate through the possible steps back until I find an edge */
    bool foundEdge = false;
    int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);

    /* TODO: For now, blocking the VDIM end of everything.  It's
     * going to be reading from the wrong place in the vapour,
     * definitely.
     * I'm trying not to think about my vapourPiece
     * discrepancy...  :/
     */
    int4 lastVapourPointCoord = makeVapourCoord(face, xdim, zdim, 0);
    float4 lastVapourPoint = readVapourPoint(vapour, units, unitOffset, lastVapourPointCoord);

    if (lastVapourPoint.w >= threshold)
    {
        /* This is an edge, and it's mine! */
        float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, 0, units[unitOffset].location);
        write_imagef(jigsawDisp, edgeCoord, edgeVertex);
        write_imagef(jigsawColour, edgeCoord, lastVapourPoint);

        /* TODO Compute the normal here. */
        write_imagef(jigsawNormal, edgeCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));

        foundEdge = true;
    }

    for (int stepsBack = 1; !foundEdge && stepsBack < EDIM; ++stepsBack)
    {
        /* Read the next point to compare with */
        int4 thisVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack);
        float4 thisVapourPoint = readVapourPoint(vapour, units, unitOffset, thisVapourPointCoord);

        /* Figure out which face it goes to, if any.
         * Upon conflict, the faces get priority in 
         * numerical order, via this looping contortion.
         */

        /* TODO For no reason I can fathom right now, all
         * variants on this logic that I've tried make
         * my AMD system hang and need the reset button.
         * :-(
         * On second thoughts, I think square pyramids are a
         * bad idea: they pretty much preclude lots of kinds of
         * irregular shapes.
         * For now, I'm going to ignore the overlap test and
         * just let overlaps happen.  I really want this kind
         * of logic to work, though.
         */
#if 0
        for (int testFace = 0; testFace < 6; ++testFace)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            /* TODO fix for `last' and `this' */
            if (thisVapourPoint.w < threshold && nextVapourPoint.w >= threshold &&
                testFace == face &&
                pointsDrawn[thisVapourPointCoord.x][thisVapourPointCoord.y][thisVapourPointCoord.z] == -1)
            {
                /* This is an edge, and it's mine! */
                float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, stepsBack, units[unitOffset].location);
                write_imagef(jigsawDisp, edgeCoord, edgeVertex);
                write_imagef(jigsawColour, edgeCoord, thisVapourPoint);

                /* TODO Compute the normal here. */
                write_imagef(jigsawNormal, edgeCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));

                foundEdge = true;
                //pointsDrawn[thisVapourPointCoord.x][thisVapourPointCoord.y][thisVapourPointCoord.z] = face;
            }
        }
#else
        if (lastVapourPoint.w < threshold && thisVapourPoint.w >= threshold)
        {
            /* This is an edge, and it's mine! */
            float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, stepsBack, units[unitOffset].location);
            write_imagef(jigsawDisp, edgeCoord, edgeVertex);
            write_imagef(jigsawColour, edgeCoord, thisVapourPoint);

            /* TODO Compute the normal here. */
            write_imagef(jigsawNormal, edgeCoord, (float4)(0.0f, 1.0f, 0.0f, 0.0f));

            foundEdge = true;
        }
#endif

        lastVapourPointCoord = thisVapourPointCoord;
        lastVapourPoint = thisVapourPoint;
    }

    if (!foundEdge)
    {
        /* This is a gap (that's normal).  Write a displacement that the
         * geometry shader will edit out.
         */
        write_imagef(jigsawDisp, edgeCoord, (float4)(NAN, NAN, NAN, NAN));
    }
}

