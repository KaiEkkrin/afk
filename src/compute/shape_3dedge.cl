/* AFK (c) Alex Holloway 2013 */

/* This program computes a 3D shape by edge detecting a
 * 3D `vapour field' (from 3dvapour) at a particular density,
 * rendering this trace into a displacement
 * texture.  This is the second part of the 3dedge stuff,
 * which takes the output of shape_3d (a 3D texture of
 * colours and object presence numbers) and maps the
 * deformable faces of a cube onto it.
 */

#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

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
    return pieceCoord * TDIM + pointCoord;
}

#endif /* AFK_FAKE3D */

struct AFK_3DEdgeComputeUnit
{
    float4 location;
    int4 vapourPiece; /* Contains 1 texel adjacency on all sides */
    int2 edgePiece; /* Points to a 3x2 grid of face textures */
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
    /* Because of the single step of adjacency in the vapour,
     * each new vapour co-ordinate starts at (1, 1, 1).
     * -1 values to xdim, zdim, and stepsBack are valid.
     */
    int4 coord = (int4)(1, 1, 1, 0);

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

float4 readVapourPoint(
    __read_only AFK_IMAGE3D vapour0,
    __read_only AFK_IMAGE3D vapour1,
    __read_only AFK_IMAGE3D vapour2,
    __read_only AFK_IMAGE3D vapour3,
    __global const struct AFK_3DEdgeComputeUnit *units,
    int unitOffset,
    int4 pieceCoord)
{
    int4 myVapourPiece = units[unitOffset].vapourPiece;
    switch (myVapourPiece.w) /* This identifies the jigsaw */
    {
    case 0:
        return read_imagef(vapour0, vapourSampler, afk_make3DJigsawCoord(myVapourPiece, pieceCoord));

    case 1:
        return read_imagef(vapour1, vapourSampler, afk_make3DJigsawCoord(myVapourPiece, pieceCoord));

    case 2:
        return read_imagef(vapour2, vapourSampler, afk_make3DJigsawCoord(myVapourPiece, pieceCoord));

    case 3:
        return read_imagef(vapour3, vapourSampler, afk_make3DJigsawCoord(myVapourPiece, pieceCoord));

    default:
        /* This really oughtn't to happen, of course... */
        return (float4)(0.0f, 0.0f, 0.0f, 0.0f);
    }
}

int2 makeEdgeJigsawCoord(__global const struct AFK_3DEdgeComputeUnit *units, int unitOffset, int face, int xdim, int zdim)
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

float3 makeEdgeVertexBase(int face, int xdim, int zdim, int stepsBack)
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
    return baseVertex;
}

/* This function makes the displacement co-ordinate at a vapour point.
 */
float4 makeEdgeVertex(int face, int xdim, int zdim, int stepsBack, float4 location)
{
    return (float4)(
        makeEdgeVertexBase(face, xdim, zdim, stepsBack) + location.xyz / location.w,
        1.0f / location.w);
}

/* This function tries to calculate a normal around a vapour
 * point and points displaced from it by the given vector.
 * It's going to be going around re-reading things that have
 * already been read but GPUs have caches, right, and this
 * kernel isn't on the critical path anyway...
 */
float4 make4PointNormal(
    __read_only AFK_IMAGE3D vapour0,
    __read_only AFK_IMAGE3D vapour1,
    __read_only AFK_IMAGE3D vapour2,
    __read_only AFK_IMAGE3D vapour3,
    __global const struct AFK_3DEdgeComputeUnit *units,
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

    float4 thisVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, thisVapourPointCoord);
    float4 xVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, xVapourPointCoord);
    float4 yVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, yVapourPointCoord);
    float4 zVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, zVapourPointCoord);

    float3 combinedVectors = (float3)(
        (xVapourPoint.w - thisVapourPoint.w) * displacement.x,
        (yVapourPoint.w - thisVapourPoint.w) * displacement.y,
        (zVapourPoint.w - thisVapourPoint.w) * displacement.z);

    return (float4)(normalize(combinedVectors), 0.0f);
}

/* This parameter list should be sufficient that I will always be able to
 * address all vapour jigsaws in the same place.  I hope!
 */
__kernel void makeShape3DEdge(
    __read_only AFK_IMAGE3D vapour0,
    __read_only AFK_IMAGE3D vapour1,
    __read_only AFK_IMAGE3D vapour2,
    __read_only AFK_IMAGE3D vapour3,
    __global const struct AFK_3DEdgeComputeUnit *units,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour,
    __write_only image2d_t jigsawNormal,
    __write_only image2d_t jigsawOverlap)
{
    const int unitOffset = get_global_id(0);
    const int xdim = get_local_id(1); /* 0..EDIM-1 */
    const int zdim = get_local_id(2); /* 0..EDIM-1 */

    /* Here I track which face(s) can claim each vapour point.
     * Indexed by (x, y, z/4), each byte contains a bitfield
     * of the faces (top bits ignored).
     */
    __local int pointsDrawn[EDIM][EDIM][EDIM/4 + 1];
    for (int i = 0; i < (EDIM/4 + 1); ++i)
    {
        pointsDrawn[xdim][zdim][i] = 0;
    }

    /* Iterate through the possible steps back until I find an edge.
     * There is one flag each in this bit field for faces 0-5 incl.
     */
    unsigned int foundEdge = 0;

    /* This tracks how many steps back each of my found edges are. */
    __local int edgeStepsBack[EDIM][EDIM][6];
    for (int i = 0; i < 6; ++i)
    {
        edgeStepsBack[xdim][zdim][i] = -1;
    }

    for (int stepsBack = 0; stepsBack < EDIM; ++stepsBack)
    {
        for (int face = 0; face < 6; ++face)
        {
            barrier(CLK_LOCAL_MEM_FENCE);
            if ((foundEdge & (1<<face)) != 0) continue;

            /* Read the next points to compare with */
            int4 lastVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack - 1);
            float4 lastVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, lastVapourPointCoord);

            int4 thisVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack);
            float4 thisVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, thisVapourPointCoord);

            barrier(CLK_LOCAL_MEM_FENCE);

            if (lastVapourPoint.w <= 0.0f && thisVapourPoint.w > 0.0f)
            {
                int zFlag = ((1<<face) << (8 * (thisVapourPointCoord.z % 4)));
                if ((atom_or(&pointsDrawn[thisVapourPointCoord.x][thisVapourPointCoord.y][thisVapourPointCoord.z/4], zFlag) & zFlag) == 0)
                {
                    /* This is an edge, write its coord. */
                    int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
                    float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, stepsBack, units[unitOffset].location);

                    write_imagef(jigsawDisp, edgeCoord, edgeVertex);
                    write_imagef(jigsawColour, edgeCoord, thisVapourPoint);

                    float4 normal = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
                    for (int xN = -1; xN <= 1; xN += 2)
                    {
                        for (int yN = -1; yN <= 1; yN += 2)
                        {
                            for (int zN = -1; zN <= 1; zN += 2)
                            {
                                normal += make4PointNormal(vapour0, vapour1, vapour2, vapour3, units, unitOffset, thisVapourPointCoord, (int4)(xN, yN, zN, 0));
                            }
                        }
                    }
    
                    write_imagef(jigsawNormal, edgeCoord, normalize(normal));
                    foundEdge |= (1<<face);
                    edgeStepsBack[xdim][zdim][face] = stepsBack;
                }
            }
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* Now, calculate which of the faces each of my edge
     * points pertains to.
     */
    for (int face = 0; face < 6; ++face)
    {
        int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);

        if ((foundEdge & (1<<face)) == 0)
        {
            /* This is a gap (that's normal).  Write a displacement that the
             * geometry shader will edit out.
             */
            write_imagef(jigsawDisp, edgeCoord, (float4)(NAN, NAN, NAN, NAN));
        }

        /* ... not an else! */
#if 1
        if (xdim < (EDIM-1) && zdim < (EDIM-1))
        {
            /* Inspect the local quad. */
            /* TODO Does this make cross-overlaps if I test the two triangles
             * separately rather than both combined?
             * ...no, apparently not.  I think this might be wrong, but it's
             * tied up with the cubes also being incomplete right now and all
             * sorts.  I should probably debug that first, otherwise, too
             * confusing.
             */
            int flaggedFirstTriangle = ((1<<6) - 1);
            int flaggedSecondTriangle = ((1<<6) - 1);

            bool flipTriangles = (face == 1 || face == 2 || face == 5);
            int firstX = (flipTriangles ? 1 : 0);
            int secondX = (flipTriangles ? 0 : 1);

            int2 firstDia = (int2)(secondX, 0);
            int2 secondDia = (int2)(firstX, 1);

            for (int x = firstX;
                x == firstX || x == secondX;
                x += (secondX - firstX))
            {
                for (int z = 0; z <= 1; ++z)
                {
                    int esb = edgeStepsBack[xdim+x][zdim+z][face];
                    if (esb >= 0 && esb < EDIM) /* TODO how the fuck is this ending up >= EDIM ?!?!?!?!?! */
                    {
                        /* Don't allow zany triangles? */
#if 0
                        if (x == firstDia.x && z == firstDia.y &&
                            abs_diff(esb, edgeStepsBack[xdim+firstX][zdim][face] > 1))
                        {
                            flaggedFirstTriangle = 0;
                        }

                        if (x == secondDia.x && z == secondDia.y &&
                            abs_diff(esb, edgeStepsBack[xdim+secondX][zdim+1][face] > 1))
                        {
                            flaggedSecondTriangle = 0;
                        }
#endif

                        int4 coord = makeVapourCoord(face, xdim+x, zdim+z, esb);
                        if (x == firstX || z == 0)
                            flaggedFirstTriangle &= ((pointsDrawn[coord.x][coord.y][coord.z/4] >> (8*(coord.z % 4))) & ((1<<6) - 1));
                        if (x == secondX || z == 1)
                            flaggedSecondTriangle &= ((pointsDrawn[coord.x][coord.y][coord.z/4] >> (8*(coord.z % 4))) & ((1<<6) - 1));
                    }
                }
            }

            uint overlap = 0;
            if ((flaggedFirstTriangle & (1<<face)) != 0 &&
                (flaggedFirstTriangle & ((1<<face) - 1)) == 0) overlap |= 1;

            if ((flaggedSecondTriangle & (1<<face)) != 0 &&
                (flaggedSecondTriangle & ((1<<face) - 1)) == 0) overlap |= 2;

            //overlap = 8;

            write_imageui(jigsawOverlap, edgeCoord, (uint4)(overlap, 0, 0, 0));
        }
#endif
    }
}

