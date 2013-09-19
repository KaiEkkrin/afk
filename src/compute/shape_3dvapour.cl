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

/* ---Like landscape_terrain--- */
__constant float maxFeatureSize = 1.0f / ((float)POINT_SUBDIVISION_FACTOR);
__constant float minFeatureSize = (1.0f / ((float)POINT_SUBDIVISION_FACTOR)) / ((float)SUBDIVISION_FACTOR);

void compute3DVapourFeature(
    float3 vl,
    float4 *vc, /* (red, green, blue, density) */
    __global const struct AFK_3DVapourFeature *features,
    int i)
{
    /* TODO: Right now I'm allowing for arbitrary overlap between
     * the features -- consider moving this to a half-cube system
     * with each feature between radius and 1-radius when I do LoD,
     * in order to be sure I can drop adjacent cubes?  Or shall I
     * just include adjacent cubes and ignore half-cubes?
     */
    float radius = (float)features[i].f[AFK_3DVF_RADIUS] * (maxFeatureSize - minFeatureSize) / 256.0f + minFeatureSize;

    float3 location = (float3)(
        (float)features[i].f[AFK_3DVF_X],
        (float)features[i].f[AFK_3DVF_Y],
        (float)features[i].f[AFK_3DVF_Z]) / 256.0f;

    float3 colour = (float3)(
        (float)features[i].f[AFK_3DVF_R],
        (float)features[i].f[AFK_3DVF_G],
        (float)features[i].f[AFK_3DVF_B]) / 256.0f;

    float weight = ((float)features[i].f[AFK_3DVF_WEIGHT] - 128.0f) / 128.0f;

    /* If this point is within the feature radius... */
    float dist = distance(location, vl);
    if (dist < radius)
    {
        /* TODO: Vary the operators here. */
        float density = weight * (radius - dist);
        if ((*vc).w < density)
        {
            /* TODO: More exciting things with colour! */
            (*vc) = (float4)(colour, density);
        }
    }
}

struct AFK_3DVapourCube
{
    float4 coord; /* x, y, z, scale */
};

__constant float reboundPoint = 100.0f;

void transformLocationToLocation(
    float3 *vl,
    float4 *vc,
    float4 fromCoord,
    float4 toCoord)
{
    *vl = (*vl * fromCoord.w + fromCoord.xyz - toCoord.xyz) / toCoord.w;

    /* If I don't weight the geometry, the finer detail dominates and
     * everything disappears.
     * However, weighting the colours causes everything to wash out
     * to white.
     * (Is there a fix to this, around understanding what a proper
     * value for `edgeThreshold' ought to be and how it should depend
     * on the LoD?)
     */
    /*
    *vc = (float4)(
        (*vc).xyz,
        (*vc).w * fromCoord.w / toCoord.w);
     */

    /* This one (and thresholding to something very small in shape_3dedge)
     * appears to work better:
     */
    /*
    *vc = (float4)(
        (*vc).xyz,
        (*vc).w - (float)THRESHOLD);
     */

    /* I'm going to refine this to try to avoid large areas of high
     * density vapour solid (which makes for boring/nonexistent geometry)
     * by trying to "rebound" the vapour from some point a bit
     * higher than the threshold:
     * TODO: I'm going to have a problem of suitably scaling up the finer
     * levels of detail here so that they actually have an effect, and no
     * doubt want to move that "normalize" so that it's applied to each
     * LoD separately, but I'll test it like this for now.
     */

    /* First, normalize the density so that the "threshold" value is 1 */
    float density = max((*vc).w / (float)THRESHOLD, 0.0f);

    /* Next, get a value that's between 0 and a "rebound point" */
    float densityMod = fmod(density, reboundPoint);

    /* Finally, invert that modulus density if `density' goes into the
     * rebound divisor an odd number of times: that should give me a
     * smooth "rebound" effect rather than instantly resetting the modulus
     * density to 0 whenever it passes the upper line
     */
    if (trunc(fmod(density / reboundPoint, reboundPoint)) != 0.0f)
        densityMod = reboundPoint - densityMod;

    *vc = (float4)(
        (*vc).xyz,
        (densityMod - 1.0f) * THRESHOLD);
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
    int4 vapourPiece;
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

    /* TODO: Colour testing. */
#if 0
    vc = (float4)(
        (float)xdim / (float)TDIM,
        (float)ydim / (float)TDIM,
        (float)zdim / (float)TDIM,
        vc.w);
#endif

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

