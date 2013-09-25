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
#if 0
        if ((*vc).w < density)
        {
            /* TODO: More exciting things with colour! */
            (*vc) = (float4)(colour, density);
        }
#else
        (*vc) += (float4)(colour, density);
#endif
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

    /* If I don't weight the geometry, the finer detail dominates and
     * everything disappears.
     * However, weighting the colours causes everything to wash out
     * to white.
     * (Is there a fix to this, around understanding what a proper
     * value for `edgeThreshold' ought to be and how it should depend
     * on the LoD?)
     */
#if 0
    *vc = (float4)(
        (*vc).xyz,
        (*vc).w * fromCoord.w / toCoord.w);
#endif

    /* This one (and thresholding to something very small in shape_3dedge)
     * appears to work better:
     * ...does it ???
     * TODO: Having a go at moving this out to something that only gets
     * run at the end of all passes.
     */
//    float density = (*vc).w - (float)THRESHOLD;
//    *vc = (float4)((*vc).xyz, clamp(density, -maxDensity, maxDensity));

#if 0
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
#endif
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

bool testFullAdjacency(int3 base, int adjacency)
{
    if (base.x >= -1 && base.x <= 1 &&
        base.y >= -1 && base.y <= 1 &&
        base.z >= -1 && base.z <= 1)
    {
        base += (int3)(1, 1, 1);
        return (adjacency & (1 << (base.x * 9 + base.y * 3 + base.z))) != 0;
    }
    else return true; /* needs to be true, otherwise we'll keep thinking,
                       * we're adjacent to fictitious gaps around the far
                       * sides.  remember "true" means "don't deform" below!
                       */
}

int3 getAdjacentFace(int3 coord, int face)
{
    switch (face)
    {
    case 0: --coord.y; break;
    case 1: --coord.x; break;
    case 2: --coord.z; break;
    case 3: ++coord.z; break;
    case 4: ++coord.x; break;
    case 5: ++coord.y; break;
    }

    return coord;
}

void transformAdjacentFaceDensity(float4 *vc, int xdim, int ydim, int zdim, int3 adjCoord, int adjacency)
{
    int closestFace = -1;
    int distanceFromClosestFace = 1<<30;

    for (int face = 0; face < 6; ++face)
    {
        int distanceFromThisFace = 1<<30;
        if (testFullAdjacency(getAdjacentFace(adjCoord, face), adjacency)) continue;

        switch (face)
        {
        case 0: distanceFromThisFace = ydim - 1; break;
        case 1: distanceFromThisFace = xdim - 1; break;
        case 2: distanceFromThisFace = zdim - 1; break;
        case 3: distanceFromThisFace = VDIM - zdim; break;
        case 4: distanceFromThisFace = VDIM - xdim; break;
        case 5: distanceFromThisFace = VDIM - ydim; break;
        }

        if (distanceFromThisFace < (VDIM / 2) &&
            distanceFromThisFace < distanceFromClosestFace)
        {
            closestFace = face;
            distanceFromClosestFace = distanceFromThisFace;
        }
    }

    if (closestFace != -1)
    {
        /* I want a gradient of the density from `faceDensity'
         * to the real value, tending towards the former at
         * the face and the latter at the middle.
         * TODO: How do I choose a good value for faceDensity ??
         * TODO: I'm also swapping in debug colours here, so that
         * I can check which faces are being gradiented and which
         * are not.
         */
        float4 vcnew = *vc;
        vcnew.xyz = (float3)(0.0f, 1.0f, 0.0f);

        float4 faceDensity = (float4)(
            1.0f, 0.0f, 1.0f,
            -THRESHOLD * FEATURE_COUNT_PER_CUBE);
        float dff = (float)distanceFromClosestFace;
        float dfc = (float)(VDIM / 2);

        vcnew = (
            faceDensity * (dfc - dff) / dfc +
            vcnew * dff / dfc);

        *vc = vcnew;
    }
}

bool inAdjacentTexel(int xdim, int ydim, int zdim, int face)
{
    switch (face)
    {
    case 0: return (ydim == 0);
    case 1: return (xdim == 0);
    case 2: return (zdim == 0);
    case 3: return (zdim >= VDIM);
    case 4: return (xdim >= VDIM);
    default: return (ydim >= VDIM);
    }
}

void transformFaceDensity(float4 *vc, int xdim, int ydim, int zdim, int adjacency)
{
    /* TODO: Is this the right place for this operation, after all? */
    float density = (*vc).w - (float)THRESHOLD;
    *vc = (float4)((*vc).xyz, clamp(density, -maxDensity, maxDensity));
    //*vc = (float4)((*vc).xyz, density);

    /* Check nearby faces for adjacency.  If there's no adjacency, I
     * want to run the density down in a gradient, so that I don't
     * have dense fog at skeleton borders.
     * Because the borders at dimension 0 are for the real cell at -1,
     * and at dimension TDIM-1 or TDIM-2 are for the real cell +1,
     * I need to do a bit of a contortion in order to calculate their
     * adjacency:
     */

    int3 adjBase = (int3)(0, 0, 0);
    /* - If I'm a centre texel and the closest face has no adjacency, do the below.
     * - If I'm a face texel and that face has no adjacency, do as per the centre texel.
     * - If I'm a face texel and that face has adjacency, test adjacency for that face
     * cube and do the below.
     * - If I'm an edge texel and neither face has adjacency, do as per the centre texel.
     * - If I'm an edge texel and one face has adjacency, do as per the face texel with adjacency.
     * - If I'm an edge texel and both faces have adjacency, test adjacency for the edge cube and
     * do the below.
     * - If I'm a corner texel...  You see the pattern now?
     */
    for (int face = 0; face < 6; ++face)
    {
        if (inAdjacentTexel(xdim, ydim, zdim, face))
        {
            int3 adjCoord = getAdjacentFace(adjBase, face);
            if (testFullAdjacency(adjCoord, adjacency)) adjBase = adjCoord;
        }
    }

    transformAdjacentFaceDensity(vc, xdim, ydim, zdim, adjBase, adjacency);
}

struct AFK_3DVapourComputeUnit
{
    float4 location;
    int4 vapourPiece;
    int adjacency;
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
     * TODO: I'm going to temporarily ignore all but the top level of
     * vapour.  This is so that I can debug the other things I'm working
     * on to turn this into a a real solid random shape.
     */
    int i;
    for (i = units[unitOffset].cubeOffset; i < (units[unitOffset].cubeOffset + units[unitOffset].cubeCount); ++i)
    {
        if (i > units[unitOffset].cubeOffset)
        {
            transformCubeToCube(&vl, &vc, cubes, i-1, i);
        }

        /* TODO: Right now the LoD conversion makes everything vanish.
         * I want extra LoDs to make a small transformation to the
         * geometry, not disappear the whole damned lot.
         * For now, I'm going to simply only compute the highest level
         * stuff, in order to verify that transformation; later, I should
         * try to add in the finer cubes, *each with proportionately smaller
         * effects*
         */
        if (cubes[i].coord.w < 12.0f) continue; /* TODO REMOVE DEBUG */
        for (int j = i * FEATURE_COUNT_PER_CUBE; j < ((i + 1) * FEATURE_COUNT_PER_CUBE); ++j)
        {
            compute3DVapourFeature(vl, &vc, features, j);
        }
    }

    /* Transform out of the space of the last cube. */
    transformLocationToLocation(&vl, &vc, cubes[i-1].coord, units[unitOffset].location);

    /* TODO: Should I do this every time, or only on the
     * last?  I've yet to be sure.
     */
    transformFaceDensity(&vc, xdim, ydim, zdim, units[unitOffset].adjacency);

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

