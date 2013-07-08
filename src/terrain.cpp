/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

#include "exception.hpp"
#include "terrain.hpp"

/* TODO REMOVEME for TARGETED_TERRAIN_DEBUG */
#include <iostream>
#include "core.hpp"

/* The methods for computing each individual
 * feature type.
 */
void AFK_TerrainFeature::compute_squarePyramid(Vec3<float>& position, Vec3<float>& colour) const
{
    if (position.v[0] >= (location.v[0] - scale.v[0]) &&
        position.v[0] < (location.v[0] + scale.v[0]) &&
        position.v[2] >= (location.v[2] - scale.v[2]) &&
        position.v[2] < (location.v[2] + scale.v[2]))
    {
        /* Make pyramid space co-ordinates. */
        float px = (position.v[0] - location.v[0]) / scale.v[0];
        float pz = (position.v[2] - location.v[2]) / scale.v[2];

        if (fabs(px) > fabs(pz))
        {
            /* We're on one of the left/right faces. */
            position.v[1] += (1.0f - fabs(px)) * scale.v[1];
        }
        else
        {
            /* We're on one of the front/back faces. */
            position.v[1] += (1.0f - fabs(pz)) * scale.v[1];
        }

        /* A temporary thing... */
        colour += tint /* * scale.v[1] */;
    }
}

AFK_TerrainFeature::AFK_TerrainFeature(const AFK_TerrainFeature& f)
{
    type        = f.type;
    location    = f.location;
    tint        = f.tint;
    scale       = f.scale;
}

AFK_TerrainFeature::AFK_TerrainFeature(
    enum AFK_TerrainType _type,
    const Vec3<float>& _location,
    const Vec3<float>& _tint,
    const Vec3<float>& _scale)
{
    type        = _type;
    location    = _location;
    tint        = _tint;
    scale       = _scale;
}

AFK_TerrainFeature& AFK_TerrainFeature::operator=(const AFK_TerrainFeature& f)
{
    type        = f.type;
    location    = f.location;
    tint        = f.tint;
    scale       = f.scale;
    return *this;
}

void AFK_TerrainFeature::compute(Vec3<float>& position, Vec3<float>& colour) const
{
    switch (type)
    {
    case AFK_TERRAIN_SQUARE_PYRAMID:
        compute_squarePyramid(position, colour);
        break;

    default:
        {
            std::ostringstream ss;
            ss << "Invalid terrain feature type: " << type;
            throw AFK_Exception(ss.str());
        }
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature)
{
    return os << "Feature(Location=" << feature.location << ", Scale=" << feature.scale << ")";
}


/* AFK_TerrainCell implementation. */

AFK_TerrainCell::AFK_TerrainCell()
{
    featureCount = 0;
}

AFK_TerrainCell::AFK_TerrainCell(const AFK_TerrainCell& c)
{
    cellCoord = c.cellCoord;
    for (unsigned int i = 0; i < c.featureCount; ++i)
        features[i] = c.features[i];
    featureCount = c.featureCount;
}

AFK_TerrainCell& AFK_TerrainCell::operator=(const AFK_TerrainCell& c)
{
    cellCoord = c.cellCoord;
    for (unsigned int i = 0; i < c.featureCount; ++i)
        features[i] = c.features[i];
    featureCount = c.featureCount;
    return *this;
}

const Vec4<float>& AFK_TerrainCell::getCellCoord(void) const
{
    return cellCoord;
}

void AFK_TerrainCell::make(
    const Vec4<float>& _cellCoord,
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    float minCellSize,
    AFK_RNG& rng,
    const Vec3<float> *forcedTint)
{
    /* This establishes where our terrain cell actually lies. */
    cellCoord = _cellCoord;

#if 0
    /* To test, only include features
     * in some small level of terrain.
     */
    if (cellCoord.v[3] > (4.0f * minCellSize))
    {
        featureCount = 0;
        return;
    }
#endif

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
     * divided by the point subdivision factor.  Like that, I
     * shouldn't get humongous feature pop-in when changing LoDs:
     * all features are minimally visible at greatest zoom.
     * (There will necessarily be a BIT of feature pop-in when
     * they add together.)
     */
    float maxFeatureSize = 1.0f / ((float)pointSubdivisionFactor);

    /* ... and the *minimum* size of a feature is equal
     * to that divided by the cell subdivision factor;
     * features smaller than that should be in subcells
     */
    float minFeatureSize = maxFeatureSize / (float)subdivisionFactor;

    /* I want between 1 and TERRAIN_FEATURE_COUNT_PER_CELL features. */
    featureCount = (descriptor % TERRAIN_FEATURE_COUNT_PER_CELL) + 1;
    descriptor = descriptor / TERRAIN_FEATURE_COUNT_PER_CELL;
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

        Vec3<float> location = afk_vec3<float>(
            rng.frand() * (maxFeatureLocationX - minFeatureLocationX) + minFeatureLocationX,
            0.0f, /* not meaningful */
            rng.frand() * (maxFeatureLocationZ - minFeatureLocationZ) + minFeatureLocationZ);
        
        Vec3<float> tint = forcedTint ? *forcedTint :
            afk_vec3<float>(rng.frand(), rng.frand(), rng.frand());

        features[i] = AFK_TerrainFeature(AFK_TERRAIN_SQUARE_PYRAMID, location, tint, scale);
    }
} 

void AFK_TerrainCell::transformCellToCell(Vec3<float>& position, const AFK_TerrainCell& other) const
{
    Vec4<float> otherCoord = other.getCellCoord();
    float scaleFactor = otherCoord.v[3] / cellCoord.v[3];

    Vec3<float> displacement = afk_vec3<float>(
        (otherCoord.v[0] - cellCoord.v[0]) / cellCoord.v[3],
        (otherCoord.v[1] - cellCoord.v[1]) / cellCoord.v[3],
        (otherCoord.v[2] - cellCoord.v[2]) / cellCoord.v[3]);

    position = (position - displacement) / scaleFactor;
}

void AFK_TerrainCell::compute(Vec3<float>& position, Vec3<float>& colour) const
{
    for (unsigned int i = 0; i < featureCount; ++i)
        features[i].compute(position, colour);
}

std::ostream& operator<<(std::ostream& os, const AFK_TerrainCell& cell)
{
    os << "TerrainCell(Coord=" << cell.cellCoord;
    for (unsigned int i = 0; i < cell.featureCount; ++i)
        os << ", " << i << "=" << cell.features[i];
    os << ")";
    return os;
}

