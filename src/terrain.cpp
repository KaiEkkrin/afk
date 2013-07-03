/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

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

void AFK_TerrainCell::make(unsigned int pointSubdivisionFactor, unsigned int subdivisionFactor, float minCellSize, AFK_RNG& rng)
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
    if (cellCoord.v[3] > (2.0f * minCellSize))
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

        Vec3<float> scale(
            rng.frand() * (maxFeatureSize - minFeatureSize) + minFeatureSize,
            ySign * (yScale * (maxFeatureSize - minFeatureSize) + minFeatureSize),
            rng.frand() * (maxFeatureSize - minFeatureSize) + minFeatureSize);

        /* To stop it from running off the sides or lifting the sides
         * up and making gaps, each feature
         * is confined to (maxFeatureSize+scale, 1.0-scale-maxFeatureSize) on the
         * (x, z) co-ordinates.
         */
        /* TODO how the F is this going wrong?!  Clamping all features to the
         * exact centre as a sanity check
         */
#if 0
        float minFeatureLocationX = scale.v[0] + maxFeatureSize;
        float maxFeatureLocationX = 1.0f - scale.v[0] - maxFeatureSize;
        float minFeatureLocationZ = scale.v[2] + maxFeatureSize;
        float maxFeatureLocationZ = 1.0f - scale.v[2] - maxFeatureSize;

        Vec3<float> location(
            rng.frand() * (maxFeatureLocationX - minFeatureLocationX) + minFeatureLocationX,
            0.0f, /* not meaningful */
            rng.frand() * (maxFeatureLocationZ - minFeatureLocationZ) + minFeatureLocationZ);
#else
        Vec3<float> location(0.5f, 0.0f, 0.5f);
#endif

        features[i] = AFK_TerrainFeature(AFK_TERRAIN_SQUARE_PYRAMID, location, scale);
    }
} 

void AFK_TerrainCell::compute(Vec3<float>& c) const
{
    for (unsigned int i = 0; i < featureCount; ++i)
        features[i].compute(c);
}


/* AFK_Terrain implementation. */

void AFK_Terrain::push(const AFK_TerrainCell& cell)
{
    t.push_front(cell);
}

void AFK_Terrain::pop()
{
    t.pop_front();
}

/* TODO This is still coming out weird.  Try sampling the terrain as
 * `double' and then backing down again afterwards.
 */
void AFK_Terrain::compute(Vec3<float>& c) const
{
    /* Try starting with the smallest feature, and progressively
     * going larger.  Hopefully this will minimise any
     * mathematical inaccuracies due to finite precision
     * arithmetic.
     */

    std::deque<AFK_TerrainCell>::const_iterator large = t.begin();
    if (large != t.end())
    {
        /* First, transform to the space of the smallest terrain cell,
         * which of course is `large' right now.
         */
        Vec3<float> csc(
            (c.v[0] - large->cellCoord.v[0]) / large->cellCoord.v[3],
            (c.v[1] - large->cellCoord.v[1]) / large->cellCoord.v[3],
            (c.v[2] - large->cellCoord.v[2]) / large->cellCoord.v[3]);

        ++large;
        std::deque<AFK_TerrainCell>::const_iterator small = t.begin();

        while (large != t.end())
        {
            /* Compute in the context of the small cell */
            small->compute(csc);

            /* Transform from the context of the small cell into the
             * context of the large cell
             */
            float scaleFactor = large->cellCoord.v[3] / small->cellCoord.v[3];
            csc = Vec3<float>(
                (csc.v[0] - (large->cellCoord.v[0] - small->cellCoord.v[0])) / scaleFactor,
                (csc.v[1] - (large->cellCoord.v[1] - small->cellCoord.v[1])) / scaleFactor,
                (csc.v[2] - (large->cellCoord.v[2] - small->cellCoord.v[2])) / scaleFactor);

            ++small;
            ++large;
        }

        /* Do the last computation */
        small->compute(csc);

        /* Transform the y co-ordinate from `small' (now biggest-cell!) space back
         * into world space
         */
        c.v[1] = csc.v[1] * small->cellCoord.v[3] + small->cellCoord.v[1];
    }
}

