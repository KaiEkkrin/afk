/* AFK (c) Alex Holloway 2013 */

/* This is the first part of the 3dedge shape stuff.
 * It combines a collection of feature cubes into a 3D
 * number field, to be edge detected by the next part
 * (shape_3dedge.cl) or maybe rendered as a volume in
 * some cunning manner.
 * I call it `vapour' because it's notionally representing
 * a kind of fog or gas (with variable density and colour).
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
    return pieceCoord * TDIM + pointCoord;
}

#endif /* AFK_FAKE3D */

/* The vapour texture's texels are (red, green, blue,
 * density).
 */

/* This kernel should run across:
 * (0..TDIM) * unitCount,
 * (0..TDIM),
 * (0..TDIM).
 */

/* TODO: The feature should get bigger, in order to
 * accommodate different types, x/y/z scales and
 * orientation, different reactions on intersection, etc.
 * For now I'll just have spheres that linearly increase
 * in weight towards the centre and max on intersection.
 */
enum AFK_3DVapourFeatureOffset
{
    AFK_3DVF_X      = 0,
    AFK_3DVF_Y      = 1,
    AFK_3DVF_Z      = 2,
    AFK_3DVF_R      = 3,
    AFK_3DVF_G      = 4,
    AFK_3DVF_B      = 5,
    AFK_3DVF_RADIUS = 6,
    AFK_3DVF_WEIGHT = 7
};

struct AFK_3DVapourFeature
{
    unsigned char f[8];
};

void compute3DVapourFeature(
    float3 vl,
    float4 *vc, /* (red, green, blue, density) */
    __global const struct AFK_3DVapourFeature *features,
    int i)
{
    float radius = (float)features[i].f[AFK_3DVF_RADIUS] * (FEATURE_MAX_SIZE - FEATURE_MIN_SIZE) / 256.0f + FEATURE_MIN_SIZE;

    float3 location = (float3)(
        (float)features[i].f[AFK_3DVF_X],
        (float)features[i].f[AFK_3DVF_Y],
        (float)features[i].f[AFK_3DVF_Z]) / 256.0f;

    float3 colour = (float3)(
        (float)features[i].f[AFK_3DVF_R],
        (float)features[i].f[AFK_3DVF_G],
        (float)features[i].f[AFK_3DVF_B]) / 256.0f;

    /* This is the second half of the weight transformation
     * described in 3d_solid
     */
    float weight = ((float)features[i].f[AFK_3DVF_WEIGHT]) / 256.0f; /* now 0-1 */
    if (weight < 0.5f) weight -= 1.0f;

    /* If this point is within the feature radius... */
    float dist = distance(location, vl);
    if (dist < radius)
    {
        /* TODO: Vary the operators here. */
        float density = weight * (radius - dist);
        (*vc) += (float4)(colour, density);
    }
}

struct AFK_3DVapourCube
{
    float4 coord; /* x, y, z, scale */
};

__constant float reboundPoint = 100.0f;
__constant float maxDensity = 2.0f * THRESHOLD * FEATURE_COUNT_PER_CUBE;

void transformLocationToLocation(
    float3 *vl,
    float4 *vc,
    float4 fromCoord,
    float4 toCoord)
{
    *vl = (*vl * fromCoord.w + fromCoord.xyz - toCoord.xyz) / toCoord.w;
}

void transformCubeToCube(
    float3 *vl,
    float4 *vc,
    __global const struct AFK_3DVapourCube *cubes,
    unsigned int cFrom,
    unsigned int cTo)
{
    float4 fromCoord = cubes[cFrom].coord;
    float4 toCoord = cubes[cTo].coord;
    transformLocationToLocation(vl, vc, fromCoord, toCoord);
}

struct AFK_3DVapourComputeUnit
{
    float4 location;
    float4 baseColour;
    int4 vapourPiece;
    int adjacency; /* TODO: Once thought I'd want this but now unused -- needs removing. */
    int cubeOffset;
    int cubeCount;
};

__kernel void makeShape3DVapour(
    __global const struct AFK_3DVapourFeature *features,
    __global const struct AFK_3DVapourCube *cubes,
    __global const struct AFK_3DVapourComputeUnit *units,
    __write_only AFK_IMAGE3D vapour0,
    __write_only AFK_IMAGE3D vapour1,
    __write_only AFK_IMAGE3D vapour2,
    __write_only AFK_IMAGE3D vapour3)
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

    /* Initialise the base points. */
    float3 vl = (float3)(
        (float)(xdim - 1) / (float)POINT_SUBDIVISION_FACTOR, 
        (float)(ydim - 1) / (float)POINT_SUBDIVISION_FACTOR, 
        (float)(zdim - 1) / (float)POINT_SUBDIVISION_FACTOR);

    /* Initialise this point's vapour numbers. */
    float4 vc = (float4)(0.0f, 0.0f, 0.0f, 0.0f);

    /* Transform into the space of the first cube. */
    transformLocationToLocation(&vl, &vc, units[unitOffset].location,
        cubes[units[unitOffset].cubeOffset].coord);

    /* Compute the number field by 
     * iterating across all of the cubes and features.
     */
    int i;
    for (i = units[unitOffset].cubeOffset; i < (units[unitOffset].cubeOffset + units[unitOffset].cubeCount); ++i)
    {
        if (i > units[unitOffset].cubeOffset)
        {
            transformCubeToCube(&vl, &vc, cubes, i-1, i);
        }

        for (int j = i * FEATURE_COUNT_PER_CUBE; j < ((i + 1) * FEATURE_COUNT_PER_CUBE); ++j)
        {
            compute3DVapourFeature(vl, &vc, features, j);
        }
    }

    /* Transform out of the space of the last cube. */
    transformLocationToLocation(&vl, &vc, cubes[i-1].coord, units[unitOffset].location);

    /* Apply the base colour in the same manner as `landscape_terrain' does,
     * and alter the density so that the edge kernel can track the zero point
     */
    vc = (float4)(
        (3.0f * units[unitOffset].baseColour.xyz + normalize(vc.xyz)) / 4.0f,
        vc.w - THRESHOLD);

    /* TODO: For now, I'm going to transfer all this into an all-float
     * image.  In future, I probably want to try to cram it into 8
     * bits per channel: this will require a reduce step to find min
     * and max values for the density.  Or maybe I want to split the
     * density off into a separate texture after all.
     * Think about this, but try it this way first because it's
     * simplest.
     */
    int4 vapourPieceCoord = (int4)(xdim, ydim, zdim, 0);
    switch (units[unitOffset].vapourPiece.w)
    {
    case 0:
        write_imagef(vapour0, afk_make3DJigsawCoord(units[unitOffset].vapourPiece, vapourPieceCoord), vc);
        break;

    case 1:
        write_imagef(vapour1, afk_make3DJigsawCoord(units[unitOffset].vapourPiece, vapourPieceCoord), vc);
        break;

    case 2:
        write_imagef(vapour2, afk_make3DJigsawCoord(units[unitOffset].vapourPiece, vapourPieceCoord), vc);
        break;

    default:
        write_imagef(vapour3, afk_make3DJigsawCoord(units[unitOffset].vapourPiece, vapourPieceCoord), vc);
        break;
    }
}

