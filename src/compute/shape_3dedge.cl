/* AFK (c) Alex Holloway 2013 */

/* This program computes a 3D shape by edge detecting a
 * 3D `vapour field' (from 3dvapour) at a particular density,
 * rendering this trace into a displacement
 * texture.  This is the second part of the 3dedge stuff,
 * which takes the output of shape_3d (a 3D texture of
 * colours and object presence numbers) and maps the
 * deformable faces of a cube onto it.
 */

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
            (TDIM-1) - stepsBack,
            0);
        break;

    case AFK_SHF_RIGHT:
        coord += (int4)(
            (TDIM-1) - stepsBack,
            xdim,
            zdim,
            0);
        break;

    case AFK_SHF_TOP:
        coord += (int4)(
            xdim,
            (TDIM-1) - stepsBack,
            zdim,
            0);
        break;
    }

    return coord;
}

int2 makeEdgeJigsawCoord(__global const struct AFK_3DComputeUnit *units, int unitOffset, int face, int xdim, int zdim)
{
    int2 baseCoord = (int2)(
        units[unitOffset].edgePiece.x * TDIM * 3,
        units[unitOffset].edgePiece.y * TDIM * 2);

    switch (face)
    {
    case AFK_SHF_BOTTOM:
        baseCoord += (int2)(xdim, zdim);
        break;

    case AFK_SHF_LEFT:
        baseCoord += (int2)(xdim + TDIM, zdim);
        break;

    case AFK_SHF_FRONT:
        baseCoord += (int2)(xdim + 2 * TDIM, zdim);
        break;

    case AFK_SHF_BACK:
        baseCoord += (int2)(xdim, zdim + TDIM);
        break;

    case AFK_SHF_RIGHT:
        baseCoord += (int2)(xdim + TDIM, zdim + TDIM);
        break;

    case AFK_SHF_TOP:
        baseCoord += (int2)(xdim + 2 * TDIM, zdim + TDIM);
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
        baseVertex = (float3)((float)xdim, (float)zdim, (float)((TDIM-1) - stepsBack));
        break;

    case AFK_SHF_RIGHT:
        baseVertex = (float3)((float)((TDIM-1) - stepsBack), (float)xdim, (float)zdim);
        break;

    case AFK_SHF_TOP:
        baseVertex = (float3)((float)xdim, (float)((TDIM-1) - stepsBack), (float)zdim);
        break;
    }

    baseVertex = (baseVertex + (float)TDIM_START) / (float)POINT_SUBDIVISION_FACTOR;
    return (float4)(
        baseVertex + location.xyz,
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
    const int unitOffset = get_global_id(0) / 6;
    const int face = get_local_id(0); /* 0..6 */
    const int xdim = get_local_id(1); /* 0..TDIM-1 */
    const int zdim = get_local_id(2); /* 0..TDIM-1 */

    /* Here I track whether each of the vapour points has already been
     * written as an edge.
     * Each word starts at -1 and is updated with which face got the
     * right to draw that edge. 
     */
    __local char pointsDrawn[TDIM-1][TDIM-1][TDIM-1];

    /* Initialize that flag array. */
    for (int y = face; y < (TDIM-1); y += face)
    {
        pointsDrawn[xdim][y][zdim] = -1;
    }

    /* Iterate through the possible steps back until I find an edge */
    bool foundEdge = false;

    int4 thisVapourPointCoord = makeVapourCoord(face, xdim, zdim, 0);
    float4 thisVapourPoint = read_imagef(vapour, vapourSampler,
        units[unitOffset].vapourPiece * TDIM + thisVapourPointCoord);

    int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);

    for (int stepsBack = 0; !foundEdge && stepsBack < (TDIM-1); ++stepsBack)
    {
        /* Read the next point to compare with */
        int4 nextVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack+1);
        float4 nextVapourPoint = read_imagef(vapour, vapourSampler,
            units[unitOffset].vapourPiece * TDIM + nextVapourPointCoord);

        /* Figure out which face it goes to, if any.
         * Upon conflict, the faces get priority in 
         * numerical order, via this looping contortion.
         */

        /* TODO For no reason I can fathom right now, all
         * variants on this logic that I've tried make
         * my AMD system hang and need the reset button.
         * :-(
         * Maybe dump the pointsDrawn test and instead just
         * try pre-assigning points to edges by means of the
         * square pyramid space division I thought of earlier?
         * (Would that be good enough?  Maybe in tandem with
         * more than one layer for each face, it would...)
         * (Also, with the square pyramid system I could try
         * shrinking the face inwards rather than culling
         * vertices that hit the pyramid edge, and taking a
         * linear sample of the vapour to get better detail
         * and fewer wasted vertices?)
         */
        for (int testFace = 0; testFace < 6; ++testFace)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

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
    }

    if (!foundEdge)
    {
        /* This is a gap (that's normal).  Write a displacement that the
         * geometry shader will edit out.
         */
        write_imagef(jigsawDisp, edgeCoord, (float4)(NAN, NAN, NAN, NAN));
    }
}

