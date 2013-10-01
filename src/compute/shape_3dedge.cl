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

/* The obvious reverse. */
void reverseVapourCoord(int face, int4 coord, int *o_xdim, int *o_zdim, int *o_stepsBack)
{
    /* Remove the single-step adjacency thingummy. */
    coord -= (int4)(1, 1, 1, 0);

    switch (face)
    {
    case AFK_SHF_BOTTOM:
        *o_xdim = coord.x;
        *o_stepsBack = coord.y;
        *o_zdim = coord.z;
        break;

    case AFK_SHF_LEFT:
        *o_stepsBack = coord.x;
        *o_xdim = coord.y;
        *o_zdim = coord.z;
        break;

    case AFK_SHF_FRONT:
        *o_xdim = coord.x;
        *o_zdim = coord.y;
        *o_stepsBack = coord.z;
        break;

    case AFK_SHF_BACK:
        *o_xdim = coord.x;
        *o_zdim = coord.y;
        *o_stepsBack = VDIM - coord.z;
        break;

    case AFK_SHF_RIGHT:
        *o_stepsBack = VDIM - coord.x;
        *o_xdim = coord.y;
        *o_zdim = coord.z;
        break;

    case AFK_SHF_TOP:
        *o_xdim = coord.x;
        *o_stepsBack = VDIM - coord.y;
        *o_zdim = coord.z;
        break;
    }
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

/* A raw normal will be correct only for the top face --
 * this fixes it for the others.
 */
float4 rotateNormal(float4 rawNormal, int face)
{
    float4 rotNormal = rawNormal;

    switch (face)
    {
    case AFK_SHF_BOTTOM : rotNormal = (float4)(rawNormal.x, -rawNormal.y, rawNormal.z, rawNormal.w); break;
    case AFK_SHF_LEFT   : rotNormal = (float4)(-rawNormal.y, rawNormal.x, rawNormal.z, rawNormal.w); break;
    case AFK_SHF_FRONT  : rotNormal = (float4)(rawNormal.x, rawNormal.z, -rawNormal.y, rawNormal.w); break;
    case AFK_SHF_BACK   : rotNormal = (float4)(rawNormal.x, -rawNormal.z, rawNormal.y, rawNormal.w); break;
    case AFK_SHF_RIGHT  : rotNormal = (float4)(rawNormal.y, -rawNormal.x, rawNormal.z, rawNormal.w); break;
    }

    return rotNormal;
}


/* Follows stuff for resolving the faces and culling triangle overlaps. */

/* In this array, we write which points are occupied by which faces. */
#define DECL_POINTS_DRAWN(pointsDrawn) __local int pointsDrawn[EDIM][EDIM][(EDIM>>2)+1]

void initPointsDrawn(DECL_POINTS_DRAWN(pointsDrawn), int xdim, int zdim)
{
    for (int i = 0; i < ((EDIM>>2) + 1); ++i)
    {
        pointsDrawn[xdim][zdim][i] = 0;
    }
}

/* Call when we're about to draw a point at the given coord and face.
 * Returns true if it hasn't been drawn yet, else false.
 */
bool setPointDrawn(DECL_POINTS_DRAWN(pointsDrawn), int4 vapourPointCoord, int face)
{
    /* `pointsDrawn' is offset from vapourPointCoord to avoid
     * the tDim gap.
     */
    int4 coord = vapourPointCoord - (int4)(1, 1, 1, 0);
    int zFlag = ((1<<face) << (8 * (coord.z & 3)));
    return ((atom_or(&pointsDrawn[coord.x][coord.y][coord.z>>2], zFlag) & zFlag) == 0);
}

/* Yanks a drawn point out of the set.  Returns true if it had been drawn,
 * else false.
 */
bool clearPointDrawn(DECL_POINTS_DRAWN(pointsDrawn), int4 vapourPointCoord, int face)
{
    int4 coord = vapourPointCoord - (int4)(1, 1, 1, 0);
    int zFlag = ((1<<face) << (8 * (coord.z & 3)));
    return ((atom_and(&pointsDrawn[coord.x][coord.y][coord.z>>2], ~zFlag) & zFlag) != 0);
}

/* In this array, we write how far back each edge point is from the face.
 * TODO: Try packing it tighter.
 */
#define DECL_EDGE_STEPS_BACK(edgeStepsBack) __local int edgeStepsBack[EDIM][EDIM][6]

#define NO_EDGE -127

void initEdgeStepsBack(DECL_EDGE_STEPS_BACK(edgeStepsBack), int xdim, int zdim)
{
    for (int i = 0; i < 6; ++i)
    {
        edgeStepsBack[xdim][zdim][i] = NO_EDGE;
    }
}

/* This enumeration identifies the different triangles that make up
 * square sections of the faces in a manner that can be OR'd together
 * to produce the final overlap texture.
 */
enum AFK_TriangleId
{
    AFK_TRI_FIRST           = 1,
    AFK_TRI_SECOND          = 2,
    AFK_TRI_FIRST_FLIPPED   = 5,
    AFK_TRI_SECOND_FLIPPED  = 6
};

/* These functions get the ids of the first and second triangle by
 * face.
 */
enum AFK_TriangleId getFirstTriangleId(int face)
{
    switch (face)
    {
    case 1: case 2: case 5:     return AFK_TRI_FIRST_FLIPPED;
    default:                    return AFK_TRI_FIRST;
    }
}

enum AFK_TriangleId getSecondTriangleId(int face)
{
    switch (face)
    {
    case 1: case 2: case 5:     return AFK_TRI_SECOND_FLIPPED;
    default:                    return AFK_TRI_SECOND;
    }
}

/* This function identifies the vapour coords of a triangle based
 * on its face coords and so forth.
 * Returns true if there is a triangle here, else false.
 */
bool makeTriangleVapourCoord(DECL_EDGE_STEPS_BACK(edgeStepsBack), int xdim, int zdim, int face, enum AFK_TriangleId id, int4 o_triCoord[3])
{
    /* Make an array of the 3 face co-ordinates that I'm
     * aiming for.
     */
    int2 faceCoord[3];

    switch (id)
    {
    case AFK_TRI_FIRST_FLIPPED:
        faceCoord[0] = (int2)(xdim+1, zdim);
        faceCoord[1] = (int2)(xdim+1, zdim+1);
        faceCoord[2] = (int2)(xdim, zdim);
        break;

    case AFK_TRI_SECOND_FLIPPED:
        faceCoord[0] = (int2)(xdim+1, zdim+1);
        faceCoord[1] = (int2)(xdim, zdim);
        faceCoord[2] = (int2)(xdim, zdim+1);

    case AFK_TRI_FIRST:
        faceCoord[0] = (int2)(xdim, zdim);
        faceCoord[1] = (int2)(xdim, zdim+1);
        faceCoord[2] = (int2)(xdim+1, zdim);
        break;

    case AFK_TRI_SECOND:
        faceCoord[0] = (int2)(xdim, zdim+1);
        faceCoord[1] = (int2)(xdim+1, zdim);
        faceCoord[2] = (int2)(xdim+1, zdim+1);
        break;
    }

    /* Next, work out the vapour coords ... */
    for (int i = 0; i < 3; ++i)
    {
        int esb = edgeStepsBack[faceCoord[i].x][faceCoord[i].y][face];
        if (esb < 0) return false;
        o_triCoord[i] = makeVapourCoord(face, faceCoord[i].x, faceCoord[i].y, esb);
    }

    return true;
}

/* This function identifies the small cube that a triangle resides in,
 * returning true (and filling out `o_cubeCoord') if it found one, or
 * false if the triangle spans cubes and therefore ought to be omitted
 * because another face could draw that part of the edge shape more
 * precisely.
 * `o_cubeCoord' comes out in vapour co-ordinates.
 */
bool triangleInSmallCube(int4 vapourCoord[3], int face, int4 *o_cubeCoord)
{
    /* The cube is at the min of the vapour coords. */
    int4 cubeCoord = (int4)(
        min(min(vapourCoord[0].x, vapourCoord[1].x), vapourCoord[2].x),
        min(min(vapourCoord[0].y, vapourCoord[1].y), vapourCoord[2].y),
        min(min(vapourCoord[0].z, vapourCoord[1].z), vapourCoord[2].z),
        0);

    int4 cubeMax = (int4)(
        max(max(vapourCoord[0].x, vapourCoord[1].x), vapourCoord[2].x),
        max(max(vapourCoord[0].y, vapourCoord[1].y), vapourCoord[2].y),
        max(max(vapourCoord[0].z, vapourCoord[1].z), vapourCoord[2].z),
        0);

    /* Check for stretched triangles. */
    if ((cubeMax.x - cubeCoord.x) > 1 ||
        (cubeMax.y - cubeCoord.y) > 1 ||
        (cubeMax.z - cubeCoord.z) > 1)
    {
        return false;
    }

    /* If the triangle is flat against one side, resize the
     * cube accordingly.
     */
    if (cubeMax.x == cubeCoord.x && face == AFK_SHF_RIGHT) --cubeCoord.x;
    if (cubeMax.y == cubeCoord.y && face == AFK_SHF_TOP) --cubeCoord.y;
    if (cubeMax.z == cubeCoord.z && face == AFK_SHF_BACK) --cubeCoord.z;

    *o_cubeCoord = cubeCoord;
    return true;
}

/* In this array, we write which triangles of which faces have been emitted,
 * indexed by the coords of the possible small cube.
 * We pack the array: 6 faces in an int, by allocating 4 bits to each face
 * using the above triangle ID enumeration plus a spare bit.
 */
#define DECL_EMITTED_TRIANGLES(emittedTriangles) __local int emittedTriangles[VDIM][VDIM][VDIM]

void initEmittedTriangles(DECL_EMITTED_TRIANGLES(emittedTriangles), int xdim, int zdim)
{
    for (int y = 0; y < VDIM; ++y)
    {
        if (xdim < VDIM && zdim < VDIM)
        {
            emittedTriangles[xdim][zdim][y] = 0;
        }
    }
}

bool testTriangleEmitted(DECL_EMITTED_TRIANGLES(emittedTriangles), int4 cubeCoord, int face, enum AFK_TriangleId id)
{
    int4 coord = cubeCoord - (int4)(1, 1, 1, 0);

    int emittedBits = (emittedTriangles[coord.x][coord.y][coord.z] >> (4 * face)) & 0xf;
    return ((emittedBits & 0x4) == (id & 0x4) &&
        ((emittedBits & 0x3) & id) != 0);
}

void setTriangleEmitted(DECL_EMITTED_TRIANGLES(emittedTriangles), int4 cubeCoord, int face, enum AFK_TriangleId id)
{
    int4 coord = cubeCoord - (int4)(1, 1, 1, 0);
    atom_or(&emittedTriangles[coord.x][coord.y][coord.z], (id << (4 * face)));
}

/* Tests whether two triangles overlap or not, assuming they're in
 * the same small cube, *and that the triangles are real triangles*
 * (no identical vertices within a triangle).
 */
bool trianglesOverlap(int4 tri1[3], int4 tri2[3])
{
    /* Count the number of identical vertices. */
    int identicalVertices = 0;

    for (int i1 = 0; i1 < 3; ++i1)
    {
        for (int i2 = 0; i2 <= 3; ++i2)
        {
            if (tri1[i1].x == tri2[i2].x &&
                tri1[i1].y == tri2[i2].y &&
                tri1[i1].z == tri2[i2].z) ++identicalVertices;
        }
    }

    switch (identicalVertices)
    {
    case 0: case 2:
        /* They definitely don't overlap. */
        return false;

    case 1:
        /* TODO: The subtle case.  For now returning true to test what happens */
        return true;

    default:
        /* They definitely overlap. */
        return true;
    }
}

/* Worker function for the below. */
bool tryOverlappingFace(
    DECL_EDGE_STEPS_BACK(edgeStepsBack),
    DECL_EMITTED_TRIANGLES(emittedTriangles),
    int4 triCoord[3],
    int4 cubeCoord,
    int lowerXdim,
    int lowerZdim,
    int lowerFace,
    enum AFK_TriangleId lowerId)
{
    int4 lowerTriCoord[3];
    if (makeTriangleVapourCoord(edgeStepsBack, lowerXdim, lowerZdim, lowerFace, lowerId, lowerTriCoord))
    {
        int4 lowerCubeCoord;
        if (triangleInSmallCube(lowerTriCoord, lowerFace, &lowerCubeCoord))
        {
            if (lowerCubeCoord.x == cubeCoord.x &&
                lowerCubeCoord.y == cubeCoord.y &&
                lowerCubeCoord.z == cubeCoord.z &&
                testTriangleEmitted(emittedTriangles, cubeCoord, lowerFace, lowerId) &&
                trianglesOverlap(triCoord, lowerTriCoord))
            {
                return false;
            }
        }
    }

    return true;
}

/* Checks whether a triangle at this location, with the given orientation,
 * would overlap.  Emits the triangle (writes it into `emittedTriangles',
 * that's all) if false.
 */
void emitIfNoOverlap(
    DECL_EDGE_STEPS_BACK(edgeStepsBack),
    DECL_EMITTED_TRIANGLES(emittedTriangles),
    int xdim,
    int zdim,
    int face,
    enum AFK_TriangleId id)
{
    /* Work out where this triangle is. */
    int4 triCoord[3];
    if (!makeTriangleVapourCoord(edgeStepsBack, xdim, zdim, face, id, triCoord)) return;

    int4 cubeCoord;
    if (!triangleInSmallCube(triCoord, face, &cubeCoord)) return;

    for (int lowerFace = 0; lowerFace < face; ++lowerFace)
    {
        /* Reconstruct the triangles emitted at this face and cube. */
        int lowerXdim, lowerZdim, lowerStepsBack;
        reverseVapourCoord(lowerFace, cubeCoord, &lowerXdim, &lowerZdim, &lowerStepsBack);

        /* TODO: This will need changing if I gain the ability to flip
         * faces dynamically.
         */
        switch (lowerFace)
        {
        case 1: case 2: case 5:
            if (!tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_FIRST) ||
                !tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_SECOND))
            {
                return;
            }
            break;

        default:
            if (!tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_FIRST_FLIPPED) ||
                !tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_SECOND_FLIPPED))
            {
                return;
            }
            break;
        }
    }

    setTriangleEmitted(emittedTriangles, cubeCoord, face, id);
}


#define TEST_CUBE 0

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

#if TEST_CUBE
    /* Important feature: Here, I produce a set of test faces
     * to use to verify that the shaders and other portions of
     * the shape render pipeline are correct.
     */
    for (int face = 0; face < 6; ++face)
    {
        int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
        float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, 0 /* stepsBack */, units[unitOffset].location);

        float4 testColour = (float4)(
            ((float)xdim / (float)EDIM),
            ((float)zdim / (float)EDIM),
            0.0f, 0.0f);

        float4 testNormal = rotateNormal((float4)(0.0f, 1.0f, 0.0f, 0.0f), face);

        /* Testing the overlap here to make sure all is
         * in order ...
         */
        uint4 testOverlap = (uint4)(3, 0, 0, 0);
        if (xdim == (EDIM-1) || zdim == (EDIM-1)) testOverlap = (uint4)(0, 0, 0, 0); /* Always ignored */

        write_imagef(jigsawDisp, edgeCoord, edgeVertex);
        write_imagef(jigsawColour, edgeCoord, testColour);
        write_imagef(jigsawNormal, edgeCoord, testNormal);
        write_imageui(jigsawOverlap, edgeCoord, testOverlap);
    }

#else
    DECL_POINTS_DRAWN(pointsDrawn);
    initPointsDrawn(pointsDrawn, xdim, zdim);

    /* Iterate through the possible steps back until I find an edge.
     * There is one flag each in this bit field for faces 0-5 incl.
     * TODO I should be able to remove this and use just
     * `edgeStepsBack'?
     */
    unsigned int foundEdge = 0;

    DECL_EDGE_STEPS_BACK(edgeStepsBack);
    initEdgeStepsBack(edgeStepsBack, xdim, zdim);

    for (int stepsBack = 0; stepsBack < (EDIM-1); ++stepsBack)
    {
        for (int face = 0; face < 6; ++face)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            /* Read the next points to compare with */
            int4 lastVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack - 1);
            float4 lastVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, lastVapourPointCoord);

            int4 thisVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack);
            float4 thisVapourPoint = readVapourPoint(vapour0, vapour1, vapour2, vapour3, units, unitOffset, thisVapourPointCoord);

#define FAKE_TEST_VAPOUR 1

#if FAKE_TEST_VAPOUR
            /* Always claiming right away should result in a cube. */
            if ((foundEdge & (1<<face)) == 0)
            {
                setPointDrawn(pointsDrawn, thisVapourPointCoord, face);

                int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
                float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, stepsBack, units[unitOffset].location);

                write_imagef(jigsawDisp, edgeCoord, edgeVertex);
                write_imagef(jigsawColour, edgeCoord, thisVapourPoint);
                write_imagef(jigsawNormal, edgeCoord, rotateNormal((float4)(0.0f, 1.0f, 0.0f, 0.0f), face));

                foundEdge |= (1<<face);
                edgeStepsBack[xdim][zdim][face] = stepsBack;
            }
#else

            if (lastVapourPoint.w <= 0.0f && thisVapourPoint.w > 0.0f)
            {
                if (setPointDrawn(pointsDrawn, thisVapourPointCoord, face))
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
    
                    write_imagef(jigsawNormal, edgeCoord, rotateNormal(normalize(normal), face));
                    foundEdge |= (1<<face);
                    edgeStepsBack[xdim][zdim][face] = stepsBack;
                }
            }
#endif /* FAKE_TEST_VAPOUR */
        }
    }


    /* In each quad, work out which faces have a complete triangle pair
     * that could be used for drawing.
     * TODO: This is all wrong, again.
     * Here is what I think I need to do (and fucking hell I hope I'm right this time):
     * - For each face, for each triangle, as below, identify the small cube
     * that it resides in.  (Make a function that does this, and returns true
     * if the triangle sits correctly in a small cube, or false if it doesn't.)
     * - For each preceding face:
     *   o For each of the two triangle in that face: (Use reverseVapourCoord().  Make
     * a function that fetches the first or second triangle.  Send that to the function,
     * above, that identifies the cube.  (Combined in triangleInSmallCube() . )
     * Verify that it's resident in the same cube as
     * the above triangle, *and that it's been emitted (a third function)*.  If so:
     *     x Count the number of common vertices.
     *       - If zero, emit the triangle.  (a fourth function.  Populates a local array
     * of emitted triangles.  Does not yet call write_imageui().)
     *       - If one, check whether the two triangles are coplanar.  (A fifth function.
     * TODO: Difficult.)  Only emit the triangle if they are not.
     *       - If two, emit the triangle.
     *       - If three, don't emit the triangle.
     * - After all that is done, iterate through the faces and their triangles again.
     * For each triangle, look up the cube, and look up the cube occupancy (the third
     * function, above).  Write the cube occupancy to the overlap texture.
     */
    DECL_EMITTED_TRIANGLES(emittedTriangles);
    initEmittedTriangles(emittedTriangles, xdim, zdim);

    for (int face = 0; face < 6; ++face)
    {
        barrier(CLK_LOCAL_MEM_FENCE);

        if (xdim < VDIM && zdim < VDIM)
        {
            /* TODO: Learn to swap faces over to dodge overlap,
             * rather than having them fixed like this.
             * After the basic thing seems OK.
             */
            switch (face)
            {
            case 1: case 2: case 5:
                /* Face is flipped. */
                emitIfNoOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_FIRST_FLIPPED);
                emitIfNoOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_SECOND_FLIPPED);
                break;

            default:
                emitIfNoOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_FIRST);
                emitIfNoOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_SECOND);
                break;
            }
        }
    }

    barrier(CLK_LOCAL_MEM_FENCE);

    /* Now, print out the overlap texture.
     * The overlap information needs to go at the first vertex
     * of the base geometry (i.e. the first vertex of the first
     * triangle as we perceive it here), not at the small cube
     * vertex.
     */ 
    for (int face = 0; face < 6; ++face)
    {
        uint overlap = 0;

        if (xdim < VDIM && zdim < VDIM)
        {
            enum AFK_TriangleId firstId, secondId;
            bool haveFirstTriangle = false;
            bool haveSecondTriangle = false;

            switch (face)
            {
            case 1: case 2: case 5:
                firstId = AFK_TRI_FIRST_FLIPPED;
                secondId = AFK_TRI_SECOND_FLIPPED;
                break;

            default:
                firstId = AFK_TRI_FIRST;
                secondId = AFK_TRI_SECOND;
                break;
            }

            int4 firstTriCoord[3];
            int4 secondTriCoord[3];
            haveFirstTriangle = makeTriangleVapourCoord(edgeStepsBack, xdim, zdim, face, firstId, firstTriCoord);
            haveSecondTriangle = makeTriangleVapourCoord(edgeStepsBack, xdim, zdim, face, secondId, secondTriCoord);

            int4 firstCubeCoord, secondCubeCoord;
            if (haveFirstTriangle &&
                triangleInSmallCube(firstTriCoord, face, &firstCubeCoord))
            {
                if (testTriangleEmitted(emittedTriangles, firstCubeCoord, face, firstId)) overlap |= 1;
            }

            if (haveSecondTriangle &&
                triangleInSmallCube(secondTriCoord, face, &secondCubeCoord))
            {
                if (testTriangleEmitted(emittedTriangles, secondCubeCoord, face, secondId)) overlap |= 2;
            }
        }

        int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
        write_imageui(jigsawOverlap, edgeCoord, (uint4)(overlap, 0, 0, 0));
    }
#endif /* TEST_CUBE */
}

