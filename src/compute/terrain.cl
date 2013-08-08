/* AFK (c) Alex Holloway 2013 */

/* This program computes the terrain across a tile. */

/* Some data types that need to be kept in
 * sync with terrain.hpp ...
 */
#define AFK_TERRAIN_CONE        2
#define AFK_TERRAIN_SPIKE       3
#define AFK_TERRAIN_HUMP        4

struct AFK_TerrainFeature
{
    float3                      tint;
    float3                      scale;
    float2                      location; /* x, z */
    int                         fType;
};

float calcDistanceToCentre(float3 *vl, float2 locationXZ)
{
    float3 location = (float3)(locationXZ.x, 0.0f, locationXZ.y);
    float3 distance = location - *vl;
    float distanceToCentreSquared =
        (distance.x * distance.x) + (distance.z * distance.z);
    float distanceToCentre = sqrt(distanceToCentreSquared);
    return distanceToCentre;
}

void computeCone(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *feature)
{
    float radius = (feature->scale.x < feature->scale.z ?
        feature->scale.x : feature->scale.z);

    float distanceToCentre = calcDistanceToCentre(vl, feature->location);

    if (distanceToCentre < radius)
    {
        float dispY = (radius - distanceToCentre) *
            (feature->scale.y / radius);
        *vl += (float3)(0.0f, dispY, 0.0f);
        *vc += feature->tint * feature->scale.z * distanceToCentre;
    }
}

void computeSpike(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *feature)
{
    float radius = (feature->scale.x < feature->scale.z ?
        feature->scale.x : feature->scale.z);

    float distanceToCentre = calcDistanceToCentre(vl, feature->location);

    /* A spike is a hump without the rounded-off section. */
    if (distanceToCentre < radius)
    {
        float dispY = (radius - distanceToCentre) *
            (radius - distanceToCentre) *
            (feature->scale.y / (radius * radius));
        *vl += (float3)(0.0f, dispY, 0.0f);
        *vc += feature->tint * feature->scale.z * distanceToCentre;
    }
}

void computeHump(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *feature)
{
    float radius = (feature->scale.x < feature->scale.z ?
        feature->scale.x : feature->scale.z);

    float distanceToCentre = calcDistanceToCentre(vl, feature->location);

    float3 disp = (float3)(0.0f, 0.0f, 0.0f);
    if (distanceToCentre < (radius / 2.0f))
    {
        /* A hump is always based off of twice the spike height at
         * distanceToCentre == (radius / 2.0f) .
         */
        disp.y = ((radius / 2.0f) * (radius / 2.0f) *
            (2.0f * feature->scale.y / (radius * radius)));

        /* From there, we subtract the inverse curve. */
        disp.y -= (distanceToCentre * distanceToCentre * feature->scale.y / (radius * radius));

        *vl += disp;
        *vc += feature->tint * feature->scale.z * distanceToCentre;
    }
    else if (distanceToCentre < radius)
    {
        disp.y = (radius - distanceToCentre) *
            (radius - distanceToCentre) *
            (feature->scale.y / (radius * radius));

        *vl += disp;
        *vc += feature->tint * feature->scale.z * distanceToCentre;
    }
}

void computeTerrainFeature(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *feature)
{
    switch (feature->fType)
    {
    case AFK_TERRAIN_CONE:
        computeCone(vl, vc, feature);
        break;

    case AFK_TERRAIN_SPIKE:
        computeSpike(vl, vc, feature);
        break;

    case AFK_TERRAIN_HUMP:
        computeHump(vl, vc, feature);
        break;
    }
}

struct AFK_TerrainTile
{
    float                       tileX;
    float                       tileZ;
    float                       tileScale;
    unsigned int                featureCount;
};

float4 getCellCoord(__global const struct AFK_TerrainTile *tiles, unsigned int t)
{
    return (float4)(tiles[t].tileX, 0.0f, tiles[t].tileZ, tiles[t].tileScale);
}

void transformTileToTile(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainTile *tiles,
    unsigned int tFrom,
    unsigned int tTo)
{
    float4 fromCoord = getCellCoord(tiles, tFrom);
    float4 toCoord = getCellCoord(tiles, tTo);

    *vl = (*vl * fromCoord.w + fromCoord.xyz - toCoord.xyz) / toCoord.w;
    *vc = *vc * fromCoord.w / toCoord.w;
}

struct AFK_TerrainComputeUnit
{
    int tileOffset;
    int tileCount;
    int2 piece;
};

/* `makeTerrain' operates across the 2 dimensions of
 * a terrain tile.
 */
__kernel void makeTerrain(
    __global const struct AFK_TerrainFeature *features,
    __global const struct AFK_TerrainTile *tiles,
    __global const struct AFK_TerrainComputeUnit *units,
    unsigned int unitOffset, /* TODO turn to `unitCount' and process all */
    __write_only image2d_t jigsaw, /* 4 floats per texel: (colour, y displacement) */
    __global float *yLowerBounds,
    __global float *yUpperBounds)
{
    const int xdim = get_global_id(0);
    const int zdim = get_global_id(1);

    /* Work out where we are inside the inputs, and inside
     * the jigsaw piece...
     */
    __global const struct AFK_TerrainComputeUnit *unit = units + unitOffset;

    /* Initialise the tile co-ordinate that corresponds to my texels
     */
    float3 vl = (float3)(
        ((float)(xdim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR),
        0.0f,
        ((float)(zdim + TDIM_START)) / ((float)POINT_SUBDIVISION_FACTOR));

    /* Initialise the colour of this co-ordinate */
    float3 vc = (float3)(0.0f, 0.0f, 0.0f);

    /* Iterate through the terrain tiles, applying
     * each tile's modification in turn.
     */
    for (int i = unit->tileOffset; i < (unit->tileOffset + unit->tileCount); ++i)
    {
        if (i > 0)
        {
            /* Transform up to next cell space. */
            transformTileToTile(&vl, &vc, tiles, i-1, i);
        }

        for (unsigned int j = 0; j < tiles[i].featureCount; ++j)
            computeTerrainFeature(&vl, &vc,
                &features[i * FEATURE_COUNT_PER_TILE + j]);
    }

    /* Make that colour space halfway sane */
    vc = normalize(vc) - 0.4f;

    /* Fill out the texels from my computed values. */
    int2 jigsawCoord = unit->piece + (int2)(xdim, zdim);
    write_imagef(jigsaw, jigsawCoord, (float4)(vc, vl.y));

    /* Now, reduce out this tile's y-bounds. */
    /* TODO: Fix this to work in the local workspace only,
     * if I change this kernel so that it hits multiple tiles
     * at once
     */

    /* TODO *2: The first one below isn't quite correct; the second one
     * is.  However, adding this functionality has made the OpenCL variant
     * go from "quite slow" to "devastatingly slow".  The thing I notice
     * is it caused me to allocate two extra buffers before each enqueue.
     * Could it be that buffer allocation is slow?
     * (Which brings me back to trying to cram all tasks into one really
     * big heapified buffer!)
     */
#if 1
    int v = xdim * TDIM + zdim;
    yLowerBounds[v + 1 << (REDUCE_ORDER - 1)] = FLT_MAX;
    yUpperBounds[v + 1 << (REDUCE_ORDER - 1)] = -FLT_MAX;
    barrier(CLK_GLOBAL_MEM_FENCE);
    yLowerBounds[v] = vl.y;
    yUpperBounds[v] = vl.y;

    /* The conclusions should end up in the very first fields
     * of each.
     */
    for (unsigned int redO = 1; redO < REDUCE_ORDER; ++redO)
    {
        barrier(CLK_GLOBAL_MEM_FENCE);
        if ((v & ((1 << redO) - 1)) == 0)
        {
            float lowerOne = yLowerBounds[v];
            float lowerTwo = yLowerBounds[v - (1 << (redO - 1))];

            float upperOne = yUpperBounds[v];
            float upperTwo = yUpperBounds[v - (1 << (redO - 1))];

            yLowerBounds[v] = (lowerOne < lowerTwo) ? lowerOne : lowerTwo;
            yUpperBounds[v] = (upperOne > upperTwo) ? upperOne : upperTwo;
        }
    }
#else
    if (zdim == 0)
    {
        yLowerBounds[xdim] = FLT_MAX;
        yUpperBounds[xdim] = -FLT_MAX;
        for (int z = 0; z < TDIM; ++z)
        {
            /* TODO The below is wrong -- I can't use `w' to read, it's
             * a write-only image !
             */
            int w = (xdim * TDIM) + z;
            if (vl.y < yLowerBounds[xdim]) yLowerBounds[xdim] = vl.y;
            else if (vl.y > yUpperBounds[xdim]) yUpperBounds[xdim] = vl.y;
        }

        barrier(CLK_GLOBAL_MEM_FENCE);
        if (xdim == 0)
        {
            for (int x = 1; x < TDIM; ++x)
            {
                if (yLowerBounds[x] < yLowerBounds[0]) yLowerBounds[0] = yLowerBounds[x];
                if (yUpperBounds[x] > yUpperBounds[0]) yUpperBounds[0] = yUpperBounds[x];
            }
        }
    }
#endif
}

