/* AFK (c) Alex Holloway 2013 */

/* This is the first part of the 3dedge shape stuff.
 * It combines a collection of feature cubes into a 3D
 * number field, to be edge detected by the next part
 * (shape_3dedge.cl) or maybe rendered as a volume in
 * some cunning manner.
 * I call it `vapour' because it's notionally representing
 * a kind of fog or gas (with variable density and colour).
 */

#pragma OPENCL EXTENSION cl_khr_3d_image_writes : enable

/* TODO: Nvidia GTX 400/500 series don't support writes to 3D
 * images.  Bollocks!  I'm going to need a bodge.  But debug
 * the actual 3D images first.
 * The bodge will probably be pretty bloody awful and ruin
 * the memory locality properties :/
 */

/* The vapour texture's texels are (red, green, blue,
 * density).
 */

/* This kernel should run across:
 * (0..VDIM) * unitCount,
 * (0..VDIM),
 * (0..VDIM).
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

    float weight = (float)features[i].f[AFK_3DVF_WEIGHT] / 256.0f;

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

void transformLocationToLocation(
    float3 *vl,
    float4 *vc,
    float4 fromCoord,
    float4 toCoord)
{
    *vl = (*vl * fromCoord.w + fromCoord.xyz - toCoord.xyz) / toCoord.w;

    /* TODO Is this the right thing to do with the density?
     * I may end up strongly following the smallest LoD and ignoring the rest.
     */
    *vc = *vc * fromCoord.w / toCoord.w;
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

struct AFK_3DComputeUnit
{
    float4 location;
    int4 vapourPiece;
    int2 edgePiece;
    int cubeOffset;
    int cubeCount;
};

__kernel void makeShape3DVapour(
    __global const struct AFK_3DVapourFeature *features,
    __global const struct AFK_3DVapourCube *cubes,
    __global const struct AFK_3DComputeUnit *units,
    __write_only image3d_t vapour)
{
    /* We're necessarily going to operate across the
     * three dimensions of a cube.
     * The first dimension should be multiplied up by
     * the unit offset, like so.
     */
    const int unitOffset = get_global_id(0) / VDIM;
    const int xdim = get_global_id(0) % VDIM;
    const int ydim = get_global_id(1);
    const int zdim = get_global_id(2);

    /* Initialise the base points. */
    float3 vl = (float3)(
        (float)xdim / (float)POINT_SUBDIVISION_FACTOR, 
        (float)ydim / (float)POINT_SUBDIVISION_FACTOR, 
        (float)zdim / (float)POINT_SUBDIVISION_FACTOR);

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

    /* TODO: For now, I'm going to transfer all this into an all-float
     * image.  In future, I probably want to try to cram it into 8
     * bits per channel: this will require a reduce step to find min
     * and max values for the density.  Or maybe I want to split the
     * density off into a separate texture after all.
     * Think about this, but try it this way first because it's
     * simplest.
     */
    int4 vapourCoord = units[unitOffset].vapourPiece * VDIM + (int4)(xdim, ydim, zdim, 0);
    write_imagef(vapour, vapourCoord, vc);
}

