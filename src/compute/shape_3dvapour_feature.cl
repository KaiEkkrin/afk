/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

/* This is the first part of the 3dedge shape stuff.
 * It combines a collection of feature cubes into a 3D
 * number field, to be edge detected by the next part
 * (shape_3dedge.cl) or maybe rendered as a volume in
 * some cunning manner.
 * I call it `vapour' because it's notionally representing
 * a kind of fog or gas (with variable density and colour).
 *
 * It requires fake3d and shape_3dvapour.
 */


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
    /* TODO: Right now I'm allowing for arbitrary overlap between
     * the features -- consider moving this to a half-cube system
     * with each feature between radius and 1-radius when I do LoD,
     * in order to be sure I can drop adjacent cubes?  Or shall I
     * just include adjacent cubes and ignore half-cubes?
     */
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

__kernel void makeShape3DVapourFeature(
    __global const struct AFK_3DVapourFeature *features,
    __global const struct AFK_3DVapourCube *cubes,
    __global const struct AFK_3DVapourComputeUnit *units,
    const int2 fake3D_size,
    const int fake3D_mult,
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

    /* Apply the base colour in the same manner as `landscape_terrain' does */
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
        write_imagef(vapour0, afk_make3DJigsawCoord(units[unitOffset].vapourPiece * TDIM, vapourPieceCoord, fake3D_size, fake3D_mult), vc);
        break;

    case 1:
        write_imagef(vapour1, afk_make3DJigsawCoord(units[unitOffset].vapourPiece * TDIM, vapourPieceCoord, fake3D_size, fake3D_mult), vc);
        break;

    case 2:
        write_imagef(vapour2, afk_make3DJigsawCoord(units[unitOffset].vapourPiece * TDIM, vapourPieceCoord, fake3D_size, fake3D_mult), vc);
        break;

    default:
        write_imagef(vapour3, afk_make3DJigsawCoord(units[unitOffset].vapourPiece * TDIM, vapourPieceCoord, fake3D_size, fake3D_mult), vc);
        break;
    }
}

