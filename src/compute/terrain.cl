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
    __global const struct AFK_TerrainFeature *features,
    int i)
{
    float radius = (features[i].scale.x < features[i].scale.z ?
        features[i].scale.x : features[i].scale.z);

    float distanceToCentre = calcDistanceToCentre(vl, features[i].location);

    if (distanceToCentre < radius)
    {
        float dispY = (radius - distanceToCentre) *
            (features[i].scale.y / radius);
        *vl += (float3)(0.0f, dispY, 0.0f);
        *vc += features[i].tint * features[i].scale.z * distanceToCentre;
    }
}

void computeSpike(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *features,
    int i)
{
    float radius = (features[i].scale.x < features[i].scale.z ?
        features[i].scale.x : features[i].scale.z);

    float distanceToCentre = calcDistanceToCentre(vl, features[i].location);

    /* A spike is a hump without the rounded-off section. */
    if (distanceToCentre < radius)
    {
        float dispY = (radius - distanceToCentre) *
            (radius - distanceToCentre) *
            (features[i].scale.y / (radius * radius));
        *vl += (float3)(0.0f, dispY, 0.0f);
        *vc += features[i].tint * features[i].scale.z * distanceToCentre;
    }
}

void computeHump(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *features,
    int i)
{
    float radius = (features[i].scale.x < features[i].scale.z ?
        features[i].scale.x : features[i].scale.z);

    float distanceToCentre = calcDistanceToCentre(vl, features[i].location);

    float3 disp = (float3)(0.0f, 0.0f, 0.0f);
    if (distanceToCentre < (radius / 2.0f))
    {
        /* A hump is always based off of twice the spike height at
         * distanceToCentre == (radius / 2.0f) .
         */
        disp.y = ((radius / 2.0f) * (radius / 2.0f) *
            (2.0f * features[i].scale.y / (radius * radius)));

        /* From there, we subtract the inverse curve. */
        disp.y -= (distanceToCentre * distanceToCentre * features[i].scale.y / (radius * radius));

        *vl += disp;
        *vc += features[i].tint * features[i].scale.z * distanceToCentre;
    }
    else if (distanceToCentre < radius)
    {
        disp.y = (radius - distanceToCentre) *
            (radius - distanceToCentre) *
            (features[i].scale.y / (radius * radius));

        *vl += disp;
        *vc += features[i].tint * features[i].scale.z * distanceToCentre;
    }
}

void computeTerrainFeature(
    float3 *vl,
    float3 *vc,
    __global const struct AFK_TerrainFeature *features,
    int i)
{
    switch (features[i].fType)
    {
    case AFK_TERRAIN_CONE:
        computeCone(vl, vc, features, i);
        break;

    case AFK_TERRAIN_SPIKE:
        computeSpike(vl, vc, features, i);
        break;

    case AFK_TERRAIN_HUMP:
        computeHump(vl, vc, features, i);
        break;
    }
}

struct AFK_TerrainTile
{
    float                       tileX;
    float                       tileZ;
    float                       tileScale;
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
    __write_only image2d_t jigsawYDisp,
    __write_only image2d_t jigsawColour)
{
    const int xdim = get_global_id(0);
    const int zdim = get_global_id(1);
    const int unitOffset = get_global_id(2);

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
    for (int i = units[unitOffset].tileOffset; i < (units[unitOffset].tileOffset + units[unitOffset].tileCount); ++i)
    {
        if (i > units[unitOffset].tileOffset)
        {
            /* Transform up to next cell space. */
            transformTileToTile(&vl, &vc, tiles, i-1, i);
        }

        for (int j = i * FEATURE_COUNT_PER_TILE; j < ((i + 1) * FEATURE_COUNT_PER_TILE); ++j)
        {
            computeTerrainFeature(&vl, &vc, features, j);
        }
    }

    /* Swap back into original tile space */
    transformTileToTile(&vl, &vc, tiles, units[unitOffset].tileOffset + units[unitOffset].tileCount - 1, units[unitOffset].tileOffset);

    /* I need to do this, otherwise brightness ends up dependent
     * on how many terrain tiles you had
     */
    vc = normalize(vc);

    /* Fill out the texels from my computed values. */
    int2 jigsawCoord = units[unitOffset].piece * TDIM + (int2)(xdim, zdim);
    write_imagef(jigsawYDisp, jigsawCoord, vl.y);
    write_imagef(jigsawColour, jigsawCoord, (float4)(vc, 0.0f));
    /* Debug colours here. */
#if 0
    write_imagef(jigsawYDisp, jigsawCoord, 0.0f);
    write_imagef(jigsawColour, jigsawCoord, (float4)(
        units[unitOffset].piece.x == 0 ? xdim : 0.0f,
        zdim,
        units[unitOffset].piece.x == 1 ? xdim : 0.0f,
        0.0f));
#endif
}

