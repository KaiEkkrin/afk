/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "computer.hpp"
#include "exception.hpp"
#include "terrain.hpp"

AFK_TerrainFeature::AFK_TerrainFeature(
    const Vec3<float>& _tint,
    const Vec3<float>& _scale,
    const Vec2<float>& _location,
    const enum AFK_TerrainType _type):
        tint(_tint), scale(_scale), location(_location), type(_type)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature)
{
    return os << "Feature(Location=" << feature.location << ", Scale=" << feature.scale << ")";
}


/* AFK_TerrainTile implementation. */

AFK_TerrainTile::AFK_TerrainTile()
{
    featureCount = 0;
}

Vec3<float> AFK_TerrainTile::getTileCoord(void) const
{
    return afk_vec3<float>(tileX, tileZ, tileScale);
}

void AFK_TerrainTile::make(
    AFK_TerrainFeature *features,
    const Vec3<float>& _tileCoord,
    const AFK_LandscapeSizes& lSizes,
    unsigned int subdivisionFactor,
    float minCellSize,
    AFK_RNG& rng,
    const Vec3<float> *forcedTint)
{
    /* This establishes where our terrain cell actually lies. */
    tileX = _tileCoord.v[0];
    tileZ = _tileCoord.v[1];
    tileScale = _tileCoord.v[2];

    /* Draw a value that tells me how many features
     * to put into this cell and what their
     * characteristics are.
     * TODO: This will want expanding a great deal!
     * Many possibilities for different ways of
     * combining features aside from simple addition
     * too: replacement, addition to a single aliased
     * value for the parent features (should look nice),
     * etc.
     */
    unsigned int descriptor = rng.uirand();

    /* The maximum size of a feature is equal to the cell size
     * divided by the feature subdivision factor.  Like that, I
     * shouldn't get humongous feature pop-in when changing LoDs:
     * all features are minimally visible at greatest zoom.
     * The feature subdivision factor should be something like the
     * point subdivision factor for the local tile (which isn't
     * necessarily the tile its features are homed to...)
     */
    float maxFeatureSize = 1.0f / ((float)lSizes.pointSubdivisionFactor);

    /* ... and the *minimum* size of a feature is equal
     * to that divided by the cell subdivision factor;
     * features smaller than that should be in subcells
     */
    float minFeatureSize = maxFeatureSize / (float)subdivisionFactor;

    /* I want between 1 and `featureCountPerTile' features. */
    featureCount = (descriptor % lSizes.featureCountPerTile) + 1;
    descriptor = descriptor / lSizes.featureCountPerTile;
    for (unsigned int i = 0; i < featureCount; ++i)
    {
        /* Pick our feature scale.
         * The y scale may be positive or negative -- draw this
         * from the descriptor.
         */
        float yScale = rng.frand();
        float ySign = (descriptor & 1) ? 1.0f : -1.0f;
        descriptor = descriptor >> 1;

        Vec3<float> scale = afk_vec3<float>(
            rng.frand() * (maxFeatureSize - minFeatureSize) + minFeatureSize,
            ySign * (yScale * (maxFeatureSize - minFeatureSize) + minFeatureSize),
            rng.frand() * (maxFeatureSize - minFeatureSize) + minFeatureSize);

        float minFeatureLocationX = scale.v[0] /* + maxFeatureSize */;
        float maxFeatureLocationX = 1.0f - scale.v[0] /* - maxFeatureSize */;
        float minFeatureLocationZ = scale.v[2] /* + maxFeatureSize */;
        float maxFeatureLocationZ = 1.0f - scale.v[2] /* - maxFeatureSize */;

        Vec2<float> location = afk_vec2<float>(
            rng.frand() * (maxFeatureLocationX - minFeatureLocationX) + minFeatureLocationX,
            rng.frand() * (maxFeatureLocationZ - minFeatureLocationZ) + minFeatureLocationZ);
        
        Vec3<float> tint = forcedTint ? *forcedTint :
            afk_vec3<float>(rng.frand(), rng.frand(), rng.frand());

        /* For now, I'm going to include one spike per tile,
         * and make the others humps.
         */
        features[i] = AFK_TerrainFeature(
            tint, scale, location, i == 0 ? AFK_TERRAIN_SPIKE : AFK_TERRAIN_HUMP);
    }
} 

std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile)
{
    os << "TerrainTile(Coord=" << tile.getTileCoord() << ")";
    return os;
}


/* AFK_TerrainList implementation. */

void AFK_TerrainList::extend(const std::vector<AFK_TerrainFeature>& features, const std::vector<AFK_TerrainTile>& tiles)
{
    f.resize(f.size() + features.size());
    std::copy(features.rbegin(), features.rend(), f.rbegin());

    t.resize(t.size() + tiles.size());
    std::copy(tiles.rbegin(), tiles.rend(), t.rbegin());
}

unsigned int AFK_TerrainList::tileCount(void) const
{
    return t.size();
}

