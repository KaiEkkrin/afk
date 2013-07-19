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
void AFK_TerrainFeature::compute_squarePyramid(Vec3<float> *positions, Vec3<float> *colours, size_t length) const
{
    for (size_t i = 0; i < length; ++i)
    {
        if (positions[i].v[0] >= (location.v[0] - scale.v[0]) &&
            positions[i].v[0] < (location.v[0] + scale.v[0]) &&
            positions[i].v[2] >= (location.v[1] - scale.v[2]) &&
            positions[i].v[2] < (location.v[1] + scale.v[2]))
        {
            /* Make pyramid space co-ordinates. */
            float px = (positions[i].v[0] - location.v[0]) / scale.v[0];
            float pz = (positions[i].v[2] - location.v[1]) / scale.v[2];

            if (fabs(px) > fabs(pz))
            {
                /* We're on one of the left/right faces. */
                positions[i].v[1] += (1.0f - fabs(px)) * scale.v[1];
            }
            else
            {
                /* We're on one of the front/back faces. */
                positions[i].v[1] += (1.0f - fabs(pz)) * scale.v[1];
            }

            /* A temporary thing... */
            colours[i] += tint /* * scale.v[1] */;
        }
    }
}

/* TODO: My goal is to replicate a landscape like this one makes, whilst
 * (a) understand how it does it, and
 * (b) not having large gaps between the bigger cells.
 */
void AFK_TerrainFeature::compute_mystery(Vec3<float> *positions, Vec3<float> *colours, size_t length) const
{
    /* Work out my feature radius. */
    float radius = (scale.v[0] < scale.v[2] ?
        (0.5f - scale.v[0]) : (0.5f - scale.v[2])) / 2.0f;

    for (size_t i = 0; i < length; ++i)
    {
        float distanceToCentreSquared =
            SQUARE(location.v[0] - positions[i].v[0]) + 
            SQUARE(location.v[1] - positions[i].v[2]);

        float distanceToCentre = sqrt(distanceToCentreSquared);
        
        if (distanceToCentre < radius)
        {
            /* I want to scale this distance so that at 0 it
             * becomes scale.v[1], and at radius it becomes 0.
             */
            float humpX = (radius - distanceToCentre) * (scale.v[1] / radius);

            positions[i].v[1] += humpX;
        }

        colours[i] += tint * scale.v[2] / 8.0f;
    }
}

void AFK_TerrainFeature::compute_hump(Vec3<float> *positions, Vec3<float> *colours, size_t length) const
{
    /* Work out my feature radius. */
    float radius = (scale.v[0] < scale.v[2] ?
        (0.5f - scale.v[0]) : (0.5f - scale.v[2]));

    for (size_t i = 0; i < length; ++i)
    {
        float distanceToCentreSquared =
            SQUARE((location.v[0] - positions[i].v[0]) / scale.v[0]) + 
            SQUARE((location.v[1] - positions[i].v[2]) / scale.v[2]);

        float distanceToCentre = sqrt(distanceToCentreSquared);
        
        if (distanceToCentre < (radius / 2.0f))
        {
            float humpX = (SQUARE(radius / 2.0f)) * (2.0f * scale.v[1] / radius) -
                distanceToCentreSquared * (scale.v[1] / radius);
            positions[i].v[1] += humpX;
        }
        else
        if (distanceToCentre < radius)
        {
            float humpX = (SQUARE(radius - distanceToCentre)) * (scale.v[1] / radius);
            positions[i].v[1] += humpX;
        }

        colours[i] += tint * scale.v[2] / 8.0f;
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
    const Vec2<float>& _location,
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

void AFK_TerrainFeature::compute(Vec3<float> *positions, Vec3<float> *colours, size_t length) const
{
    switch (type)
    {
    case AFK_TERRAIN_SQUARE_PYRAMID:
        compute_squarePyramid(positions, colours, length);
        break;

    case AFK_TERRAIN_MYSTERY:
        compute_mystery(positions, colours, length);
        break;

    case AFK_TERRAIN_HUMP:
        compute_hump(positions, colours, length);
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


/* AFK_TerrainTile implementation. */

AFK_TerrainTile::AFK_TerrainTile()
{
    featureCount = 0;
}

AFK_TerrainTile::AFK_TerrainTile(const AFK_TerrainTile& c)
{
    tileCoord = c.tileCoord;
    for (unsigned int i = 0; i < c.featureCount; ++i)
        features[i] = c.features[i];
    featureCount = c.featureCount;
}

AFK_TerrainTile& AFK_TerrainTile::operator=(const AFK_TerrainTile& c)
{
    tileCoord = c.tileCoord;
    for (unsigned int i = 0; i < c.featureCount; ++i)
        features[i] = c.features[i];
    featureCount = c.featureCount;
    return *this;
}

const Vec3<float>& AFK_TerrainTile::getTileCoord(void) const
{
    return tileCoord;
}

Vec4<float> AFK_TerrainTile::getCellCoord(void) const
{
    return afk_vec4<float>(
        tileCoord.v[0], 0.0f, tileCoord.v[1], tileCoord.v[2]);
}

void AFK_TerrainTile::make(
    const Vec3<float>& _tileCoord,
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    float minCellSize,
    AFK_RNG& rng,
    const Vec3<float> *forcedTint)
{
    /* This establishes where our terrain cell actually lies. */
    tileCoord = _tileCoord;

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

    /* I want between 1 and TERRAIN_FEATURE_COUNT_PER_TILE features. */
    featureCount = (descriptor % TERRAIN_FEATURE_COUNT_PER_TILE) + 1;
    descriptor = descriptor / TERRAIN_FEATURE_COUNT_PER_TILE;
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

        features[i] = AFK_TerrainFeature(AFK_TERRAIN_TYPE_IN_USE, location, tint, scale);
    }
} 

void AFK_TerrainTile::transformTileToTile(Vec3<float> *positions, size_t length, const AFK_TerrainTile& from) const
{
    Vec4<float> toCoord = getCellCoord();
    Vec4<float> fromCoord = from.getCellCoord();
    float scaleFactor = fromCoord.v[3] / toCoord.v[3];

    Vec3<float> displacement = afk_vec3<float>(
        (fromCoord.v[0] - toCoord.v[0]) / toCoord.v[3],
        (fromCoord.v[1] - toCoord.v[1]) / toCoord.v[3],
        (fromCoord.v[2] - toCoord.v[2]) / toCoord.v[3]);

    for (size_t i = 0; i < length; ++i)
        positions[i] = (positions[i] - displacement) / scaleFactor;
}

void AFK_TerrainTile::compute(Vec3<float> *positions, Vec3<float> *colours, size_t length) const
{
    for (unsigned int i = 0; i < featureCount; ++i)
        features[i].compute(positions, colours, length);
}

std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile)
{
    os << "TerrainTile(Coord=" << tile.tileCoord;
    for (unsigned int i = 0; i < tile.featureCount; ++i)
        os << ", " << i << "=" << tile.features[i];
    os << ")";
    return os;
}


/* AFK_TerrainList implementation. */

AFK_TerrainList::AFK_TerrainList()
{
    /* Stop us from doing lots of small resizes when creating
     * the list.
     */
    t.reserve(16);
}

void AFK_TerrainList::add(boost::shared_ptr<AFK_TerrainTile> tile)
{
    t.push_back(tile);
}

void AFK_TerrainList::compute(Vec3<float> *positions, Vec3<float> *colours, size_t length) const
{
    /* Sanity check. */
    if (t.size() == 0) return;

    /* Transform into the space of the first tile. */
    Vec4<float> bottomCellCoord = t[0]->getCellCoord();
    Vec3<float> bottomCellXYZ = afk_vec3<float>(bottomCellCoord.v[0], bottomCellCoord.v[1], bottomCellCoord.v[2]);
    for (size_t i = 0; i < length; ++i)
        positions[i] = (positions[i] - bottomCellXYZ) / bottomCellCoord.v[3];

    for (unsigned int i = 0; i < t.size(); ++i)
    {
        if (i > 0)
        {
            /* Transform the terrain up to the next cell space. */
            t[i-1]->transformTileToTile(positions, length, *(t[i]));
        }

        t[i]->compute(positions, colours, length);
    }

    /* At the top level, transform back into world space. */
    Vec4<float> topCellCoord = t[t.size()-1]->getCellCoord();
    Vec3<float> topCellXYZ = afk_vec3<float>(topCellCoord.v[0], topCellCoord.v[1], topCellCoord.v[2]);

    for (size_t i = 0; i < length; ++i)
        positions[i] = (positions[i] * topCellCoord.v[3]) + topCellXYZ;
}

