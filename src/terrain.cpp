/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

#include "core.hpp"
#include "exception.hpp"
#include "terrain.hpp"

/* The methods for computing each individual
 * feature type.
 */
void AFK_TerrainFeature::compute_squarePyramid(Vec3<float>& c) const
{
    if (c.v[0] >= (location.v[0] - scale.v[0]) &&
        c.v[0] < (location.v[0] + scale.v[0]) &&
        c.v[2] >= (location.v[2] - scale.v[2]) &&
        c.v[2] < (location.v[2] + scale.v[2]))
    {
        /* Make pyramid space co-ordinates. */
        float px = (c.v[0] - location.v[0]) / scale.v[0];
        float pz = (c.v[2] - location.v[2]) / scale.v[2];

        if (fabs(px) > fabs(pz))
        {
            /* We're on one of the left/right faces. */
            c.v[1] += (1.0f - fabs(px)) * scale.v[1];
        }
        else
        {
            /* We're on one of the front/back faces. */
            c.v[1] += (1.0f - fabs(pz)) * scale.v[1];
        }
    }
}

AFK_TerrainFeature::AFK_TerrainFeature(const AFK_TerrainFeature& f)
{
    type        = f.type;
    location    = f.location;
    scale       = f.scale;
}

AFK_TerrainFeature::AFK_TerrainFeature(
    enum AFK_TerrainType _type,
    const Vec3<float>& _location,
    const Vec3<float>& _scale)
{
    type        = _type;
    location    = _location;
    scale       = _scale;
}

AFK_TerrainFeature& AFK_TerrainFeature::operator=(const AFK_TerrainFeature& f)
{
    type        = f.type;
    location    = f.location;
    scale       = f.scale;
    return *this;
}

void AFK_TerrainFeature::compute(Vec3<float>& c) const
{
    switch (type)
    {
    case AFK_TERRAIN_SQUARE_PYRAMID:
        compute_squarePyramid(c);
        break;

    default:
        {
            std::ostringstream ss;
            ss << "Invalid terrain feature type: " << type;
            throw AFK_Exception(ss.str());
        }
    }
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

AFK_TerrainCell::AFK_TerrainCell(const Vec4<float>& coord)
{
    cellCoord = coord;
    featureCount = 0;
}

AFK_TerrainCell& AFK_TerrainCell::operator=(const AFK_TerrainCell& c)
{
    cellCoord = c.cellCoord;
    for (unsigned int i = 0; i < c.featureCount; ++i)
        features[i] = c.features[i];
    featureCount = c.featureCount;
    return *this;
}

void AFK_TerrainCell::make(AFK_RNG& rng)
{
#if 1
    /* To test, I'm only going to include features
     * in the smallest level of terrain.
     * TODO: I need to sort out the terrain composition so
     * that it does something sensible about crossing cell
     * boundaries (or really really makes sure it can't, and
     * I don't know how to do that, because I want to be able
     * to go down as well as up, and I'm right next to the
     * zero y at the moment as it is, and right now every
     * landscape cell has a plane of points in it if I let it).
     * However, the array of points is already making it very
     * difficult to visualise what is going on, so before I
     * continue, I should first turn it into a properly lit
     * polygonal landscape so that I can actually see what I've
     * made.
     */
    if (cellCoord.v[3] > (2.0f * afk_core.landscape->minCellSize))
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

    /* So as not to run out of the cell, the maximum
     * size for a feature is equal to 1 divided by
     * the number of features that could appear there.
     * The half cells mechanism means any point could
     * have up to 2 features on it.
     */
    float featureScaleFactor = 1.0f / (2.0f * TERRAIN_FEATURE_COUNT_PER_CELL);

    /* So that they don't run off the sides, each feature
     * is confined to (featureScaleFactor, 1.0-featureScaleFactor)
     * on the x, z co-ordinates.
     * That means the x or z location is featureScaleFactor +
     * (0.0-1.0)*featureLocationFactor in cell co-ordinates
     */
    float featureLocationFactor = 1.0f - (2.0f * featureScaleFactor);

    featureCount = descriptor % (TERRAIN_FEATURE_COUNT_PER_CELL + 1) + 1; /* between 1 and TERRAIN_FEATURE_COUNT_PER_CELL */
    for (unsigned int i = 0; i < featureCount; ++i)
    {
        features[i] = AFK_TerrainFeature(
            AFK_TERRAIN_SQUARE_PYRAMID,
            Vec3<float>( /* location */
                rng.frand() * featureLocationFactor + featureScaleFactor,
                0.0f,
                rng.frand() * featureLocationFactor + featureScaleFactor),
            Vec3<float>( /* scale */
                rng.frand() * featureScaleFactor,
                rng.frand() * featureScaleFactor,
                rng.frand() * featureScaleFactor));
    }
} 

bool AFK_TerrainCell::compute(Vec3<float>& c) const
{
    bool haveTerrain = false;

    /* Make cell-space co-ordinates for `c' */
    Vec3<float> csc(
        (c.v[0] - cellCoord.v[0]) / cellCoord.v[3],
        (c.v[1] - cellCoord.v[1]) / cellCoord.v[3],
        (c.v[2] - cellCoord.v[2]) / cellCoord.v[3]);

    /* Check whether the sample point is in this terrain cell
     * at all
     */
    if (csc.v[0] >= 0.0f && csc.v[0] < 1.0f &&
        csc.v[1] >= 0.0f && csc.v[1] < 1.0f &&
        csc.v[2] >= 0.0f && csc.v[2] < 1.0f)
    {
        /* Compute the terrain on the cell-space */
        for (unsigned int i = 0; i < featureCount; ++i)
            features[i].compute(csc);

        /* Wrap the computed terrain so that it stays within
         * this cell
         * TODO: Finesse as described above
         */
        csc.v[0] = fmod(csc.v[0], 1.0f);
        csc.v[1] = fmod(csc.v[1], 1.0f);
        csc.v[2] = fmod(csc.v[2], 1.0f);

        /* Update `c' with the transformed terrain and notify
         * the caller that we found terrain here.
         */
        c = Vec3<float>(
            csc.v[0] * cellCoord.v[3] + cellCoord.v[0],
            csc.v[1] * cellCoord.v[3] + cellCoord.v[1],
            csc.v[2] * cellCoord.v[3] + cellCoord.v[2]);
        haveTerrain = true;
    }

    return haveTerrain;
}


/* AFK_Terrain implementation. */

void AFK_Terrain::init(unsigned int maxSubdivisions)
{
    /* Multiplying by 5 to accommodate halfcell terrain. */
    t = std::vector<AFK_TerrainCell>(maxSubdivisions * 5);
}

void AFK_Terrain::push(const AFK_TerrainCell& cell)
{
    t.push_back(cell);
}

void AFK_Terrain::pop()
{
    t.pop_back();
}

bool AFK_Terrain::compute(Vec3<float>& c) const
{
    bool haveTerrain = false;
    for (std::vector<AFK_TerrainCell>::const_iterator tIt = t.begin(); tIt != t.end(); ++tIt)
        haveTerrain = tIt->compute(c);

    return haveTerrain;
}

