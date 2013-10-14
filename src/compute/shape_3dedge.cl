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

/* This program computes a 3D shape by edge detecting a
 * 3D `vapour field' (from 3dvapour) at a particular density,
 * rendering this trace into a displacement
 * texture.  This is the second part of the 3dedge stuff,
 * which takes the output of shape_3d (a 3D texture of
 * colours and object presence numbers) and maps the
 * deformable faces of a cube onto it.
 *
 * It requires fake3d.
 */


#pragma OPENCL EXTENSION cl_khr_local_int32_extended_atomics : enable

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
    __read_only AFK_IMAGE3D vapour,
    const int2 fake3D_size,
    const int fake3D_mult,
    __global const struct AFK_3DEdgeComputeUnit *units,
    int unitOffset,
    int4 pieceCoord)
{
    return read_imagef(vapour, vapourSampler, afk_make3DJigsawCoord(
        units[unitOffset].vapourPiece * TDIM, pieceCoord, fake3D_size, fake3D_mult));
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
    __read_only AFK_IMAGE3D vapour,
    const int2 fake3D_size,
    const int fake3D_mult,
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

    float4 thisVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, thisVapourPointCoord);
    float4 xVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, xVapourPointCoord);
    float4 yVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, yVapourPointCoord);
    float4 zVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, zVapourPointCoord);

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

/* In this array, we write how far back each edge point is from the face.
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
        break;

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

bool verticesAreEqual(int4 v1, int4 v2)
{
    return (v1.x == v2.x && v1.y == v2.y && v1.z == v2.z);
}

/* Tests whether two triangles are coplanar. */
bool trianglesAreCoplanar(int4 tri1[3], int4 tri2[3])
{
    float3 fTri1[3];
    float3 fTri2[3];

    for (int i = 0; i < 3; ++i)
    {
        fTri1[i] = (float3)((float)tri1[i].x, (float)tri1[i].y, (float)tri1[i].z);
        fTri2[i] = (float3)((float)tri2[i].x, (float)tri2[i].y, (float)tri2[i].z);
    }

    float3 fNorm1 = cross(fTri1[2] - fTri1[0], fTri1[1] - fTri1[0]);

    /* I put a small error margin in here just in case. */
    return (fabs(dot(fNorm1, fTri2[2] - fTri2[0])) < 0.001f &&
        fabs(dot(fNorm1, fTri2[1] - fTri2[0])) < 0.001f);
}


/* Tests whether two triangles overlap or not, assuming they're in
 * the same small cube, *and that the triangles are real triangles*
 * (no identical vertices within a triangle).
 */
bool trianglesOverlap(int4 tri1[3], int4 tri2[3])
{
    /* Count the number of identical vertices and
     * keep track of which ones they are.
     */
    int identicalVertices = 0;
    int4 identical[3];

    for (int i1 = 0; i1 < 3; ++i1)
    {
        for (int i2 = 0; i2 <= 3; ++i2)
        {
            if (verticesAreEqual(tri1[i1], tri2[i2]))
            {
                identical[identicalVertices] = tri1[i1];
                ++identicalVertices;
            }
        }
    }

    switch (identicalVertices)
    {
    case 0:
        /* They definitely don't overlap. */
        return false;

    case 1:
        return trianglesAreCoplanar(tri1, tri2);

    case 2:
        if (trianglesAreCoplanar(tri1, tri2))
        {
            /* Dig up the different vertex pair. */
            int4 diff1, diff2;
            for (int i = 0; i < 3; ++i)
            {
                if (!verticesAreEqual(tri1[i], identical[0]) &&
                    !verticesAreEqual(tri1[i], identical[1]))
                {
                    diff1 = tri1[i];
                }

                if (!verticesAreEqual(tri2[i], identical[0]) &&
                    !verticesAreEqual(tri2[i], identical[1]))
                {
                    diff2 = tri2[i];
                }
            }

            /* The triangles overlap only if the two different vertices
             * don't form the diagonal of the square.
             */
            float3 fId0 = (float3)((float)identical[0].x, (float)identical[0].y, (float)identical[0].z);
            float3 fId1 = (float3)((float)identical[1].x, (float)identical[1].y, (float)identical[1].z);
            float3 fDiff1 = (float3)((float)diff1.x, (float)diff1.y, (float)diff1.z);
            float3 fDiff2 = (float3)((float)diff2.x, (float)diff2.y, (float)diff2.z);

            return (distance(fId0, fId1) <= distance(fId0, fDiff1) ||
                distance(fId0, fId1) <= distance(fId0, fDiff2));
        }
        else return false;

    default:
        /* They definitely overlap. */
        return true;
    }
}

/* Worker function for the below.  Returns false if these faces overlap,
 * else true.
 */
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
 * would overlap.  If it wouldn't, fills out `o_cubeCoord' with the cube for
 * this triangle, and returns true; if it would, or the triangle isn't valid,
 * returns false.
 */
bool noOverlap(
    DECL_EDGE_STEPS_BACK(edgeStepsBack),
    DECL_EMITTED_TRIANGLES(emittedTriangles),
    int xdim,
    int zdim,
    int face,
    enum AFK_TriangleId id,
    int4 *o_cubeCoord)
{
    /* Work out where this triangle is. */
    int4 triCoord[3];
    if (!makeTriangleVapourCoord(edgeStepsBack, xdim, zdim, face, id, triCoord)) return false;

    int4 cubeCoord;
    if (!triangleInSmallCube(triCoord, face, &cubeCoord)) return false;

    for (int lowerFace = 0; lowerFace < face; ++lowerFace)
    {
        /* Reconstruct the triangles emitted at this face and cube. */
        int lowerXdim, lowerZdim, lowerStepsBack;
        reverseVapourCoord(lowerFace, cubeCoord, &lowerXdim, &lowerZdim, &lowerStepsBack);

        if (!tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_FIRST) ||
            !tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_SECOND) ||
            !tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_FIRST_FLIPPED) ||
            !tryOverlappingFace(edgeStepsBack, emittedTriangles, triCoord, cubeCoord, lowerXdim, lowerZdim, lowerFace, AFK_TRI_SECOND_FLIPPED))
        {
            return false;
        }
    }

    *o_cubeCoord = cubeCoord;
    return true;
}

#define FAKE_COLOURS 0

#if FAKE_COLOURS
/* This debug function supplies a useful false colour. */
float4 getFakeColour(int face)
{
    switch (face)
    {
    case 0:     return (float4)(1.0f, 0.0f, 0.0f, 0.0f);
    case 1:     return (float4)(0.0f, 1.0f, 0.0f, 0.0f);
    case 2:     return (float4)(0.0f, 0.0f, 1.0f, 0.0f);
    case 3:     return (float4)(0.0f, 1.0f, 1.0f, 0.0f);
    case 4:     return (float4)(1.0f, 0.0f, 1.0f, 0.0f);
    default:    return (float4)(1.0f, 1.0f, 0.0f, 0.0f);
    }
}
#endif


/* This parameter list should be sufficient that I will always be able to
 * address all vapour jigsaws in the same place.  I hope!
 *
 * TODO: To change this kernel so that it outputs data that allows the
 * shader to read the rest from the vapour -- and so that it can allow
 * the shader to instance more triangles on a case by case basis to fill
 * gaps, rather than needing several times the base geometry:
 * - Colour and normal are read from the vapour.
 * - jigsawDisp becomes a base displacement, pointing to the front of
 * the face.  (this needs to remain a 4-component float32.)
 * - jigsawOverlap is now two channels.  Each of these channels is a uint32
 * packed with a sequence of 4 bit lumps, from the little end up to the big
 * end, or zeroes if nothing is applicable:
 *   o (red channel) overlap info as currently present, but packed as above
 *   o (green channel) edge steps back.
 * This limits EDIM to 16, but that's okay.
 * This also kills the test cube off, and if I want that back (which I
 * probably do), I'll need to implement one in the vapour.
 */
__kernel void makeShape3DEdge(
    __read_only AFK_IMAGE3D vapour,
    __global const struct AFK_3DEdgeComputeUnit *units,
    const int2 fake3D_size,
    const int fake3D_mult,
    __write_only image2d_t jigsawDisp,
    __write_only image2d_t jigsawColour,
    __write_only image2d_t jigsawNormal,
    __write_only image2d_t jigsawOverlap)
{
    const int unitOffset = get_global_id(0);
    const int xdim = get_local_id(1); /* 0..EDIM-1 */
    const int zdim = get_local_id(2); /* 0..EDIM-1 */

    /* Iterate through the possible steps back until I find an edge.
     */
    DECL_EDGE_STEPS_BACK(edgeStepsBack);
    initEdgeStepsBack(edgeStepsBack, xdim, zdim);

    for (int stepsBack = 0; stepsBack < (EDIM-1); ++stepsBack)
    {
        for (int face = 0; face < 6; ++face)
        {
            barrier(CLK_LOCAL_MEM_FENCE);

            /* Read the next points to compare with */
            int4 lastVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack - 1);
            float4 lastVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, lastVapourPointCoord);

            int4 thisVapourPointCoord = makeVapourCoord(face, xdim, zdim, stepsBack);
            float4 thisVapourPoint = readVapourPoint(vapour, fake3D_size, fake3D_mult, units, unitOffset, thisVapourPointCoord);

#define FAKE_TEST_VAPOUR 0

#if FAKE_TEST_VAPOUR
            /* Always claiming right away should result in a cube. */
            if (edgeStepsBack[xdim][zdim][face] == NO_EDGE)
            {
                int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
                float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, stepsBack, units[unitOffset].location);

                write_imagef(jigsawDisp, edgeCoord, edgeVertex);
#if FAKE_COLOURS
                write_imagef(jigsawColour, edgeCoord, getFakeColour(face));
#else
                write_imagef(jigsawColour, edgeCoord, thisVapourPoint);
#endif
                write_imagef(jigsawNormal, edgeCoord, rotateNormal((float4)(0.0f, 1.0f, 0.0f, 0.0f), face));

                edgeStepsBack[xdim][zdim][face] = stepsBack;
            }
#else
            if (edgeStepsBack[xdim][zdim][face] == NO_EDGE &&
                lastVapourPoint.w <= 0.0f && thisVapourPoint.w > 0.0f)
            {
                 /* This is an edge, write its coord. */
                 int2 edgeCoord = makeEdgeJigsawCoord(units, unitOffset, face, xdim, zdim);
                 float4 edgeVertex = makeEdgeVertex(face, xdim, zdim, stepsBack, units[unitOffset].location);

                 write_imagef(jigsawDisp, edgeCoord, edgeVertex);
#if FAKE_COLOURS
                 write_imagef(jigsawColour, edgeCoord, getFakeColour(face));
#else
                 write_imagef(jigsawColour, edgeCoord, thisVapourPoint);
#endif

                float4 normal = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
                for (int xN = -1; xN <= 1; xN += 2)
                {
                    for (int yN = -1; yN <= 1; yN += 2)
                    {
                        for (int zN = -1; zN <= 1; zN += 2)
                        {
                            normal += make4PointNormal(vapour, fake3D_size, fake3D_mult, units, unitOffset, thisVapourPointCoord, (int4)(xN, yN, zN, 0));
                        }
                    }
                }

                write_imagef(jigsawNormal, edgeCoord, (float4)(normalize(normal.xyz), 0.0f));
                edgeStepsBack[xdim][zdim][face] = stepsBack;
            }
#endif /* FAKE_TEST_VAPOUR */
        }
    }


    /* In each quad, work out which faces have a complete triangle pair
     * that could be used for drawing.  (These are the notes I made before I wrote
     * the code, so they may seem a little out of sync now :) )
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
     *       - If one, check whether the two triangles are coplanar.  (A fifth function.)
     * Only emit the triangle if they are not.
     *       - If two, emit the triangle.
     *       - If three, don't emit the triangle.
     * - After all that is done, iterate through the faces and their triangles again.
     * For each triangle, look up the cube, and look up the cube occupancy (the third
     * function, above).  Write the cube occupancy to the overlap texture.
     */
    DECL_EMITTED_TRIANGLES(emittedTriangles);
    initEmittedTriangles(emittedTriangles, xdim, zdim);

    /* Here, track which faces I flipped.  Bit field.  */
    int flipped = 0;

    for (int face = 0; face < 6; ++face)
    {
        barrier(CLK_LOCAL_MEM_FENCE);

        if (xdim < VDIM && zdim < VDIM)
        {
            /* Work out both flipped and non-flipped variants.  Choose the one that
             * can emit the most triangles.
             */
            int4 firstCubeCoord, secondCubeCoord, firstFlippedCubeCoord, secondFlippedCubeCoord;

            /* This is a bit field: 1, 2, 4, 8 for the 4 triangles in the above order. */
            int possibleEmits = 0;

            int nonFlippedEmits = 0;
            int flippedEmits = 0;

            if (noOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_FIRST, &firstCubeCoord))
            {
                possibleEmits |= 1;
                ++nonFlippedEmits;
            }

            if (noOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_SECOND, &secondCubeCoord))
            {
                possibleEmits |= 2;
                ++nonFlippedEmits;
            }

            if (noOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_FIRST_FLIPPED, &firstFlippedCubeCoord))
            {
                possibleEmits |= 4;
                ++flippedEmits;
            }

            if (noOverlap(edgeStepsBack, emittedTriangles, xdim, zdim, face, AFK_TRI_SECOND_FLIPPED, &secondFlippedCubeCoord))
            {
                possibleEmits |= 8;
                ++flippedEmits;
            }

            if (nonFlippedEmits > flippedEmits)
            {
                if ((possibleEmits & 1) != 0) setTriangleEmitted(emittedTriangles, firstCubeCoord, face, AFK_TRI_FIRST);
                if ((possibleEmits & 2) != 0) setTriangleEmitted(emittedTriangles, secondCubeCoord, face, AFK_TRI_SECOND);
            }
            else
            {
                if ((possibleEmits & 4) != 0) setTriangleEmitted(emittedTriangles, firstFlippedCubeCoord, face, AFK_TRI_FIRST_FLIPPED);
                if ((possibleEmits & 8) != 0) setTriangleEmitted(emittedTriangles, secondFlippedCubeCoord, face, AFK_TRI_SECOND_FLIPPED);
                flipped |= (1<<face);
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

            if ((flipped & (1<<face)) != 0)
            {
                firstId = AFK_TRI_FIRST_FLIPPED;
                secondId = AFK_TRI_SECOND_FLIPPED;
                overlap = 4;
            }
            else
            {
                firstId = AFK_TRI_FIRST;
                secondId = AFK_TRI_SECOND;
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
        write_imageui(jigsawOverlap, edgeCoord, (uint4)(overlap, edgeStepsBack[xdim][zdim][face], 0, 0));
    }
}

