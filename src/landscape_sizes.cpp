/* AFK (c) Alex Holloway 2013 */

#include "def.hpp"
#include "landscape_sizes.hpp"


AFK_LandscapeSizes::AFK_LandscapeSizes(unsigned int subdivisionFactor, unsigned int pointSubdivisionFactor):
    pointSubdivisionFactor(pointSubdivisionFactor),
    vDim(pointSubdivisionFactor + 1), /* one extra vertex along the top and right sides to join with the adjacent tile */
    iDim(pointSubdivisionFactor),
    tDim(pointSubdivisionFactor + 3), /* one extra vertex either side, to smooth colours and normals; */
                                      /* another extra vertex on +x and +z, to smooth the join-triangle normals */
    tDimStart(-1),                    /* begin one index before the origin to do the bottom and left smoothing */
    vCount(SQUARE(pointSubdivisionFactor + 1)),
    iCount(SQUARE(pointSubdivisionFactor) * 2), /* two triangles per vertex */
    tCount(SQUARE(pointSubdivisionFactor + 3)),
    vSize(SQUARE(pointSubdivisionFactor + 1) * sizeof(Vec3<float>)),
    iSize(SQUARE(pointSubdivisionFactor) * 2 * 3 * sizeof(unsigned short)),
    tSize(SQUARE(pointSubdivisionFactor + 3) * 2 * sizeof(Vec4<float>)), /* normal + y disp., colour */
    featureCountPerTile(SQUARE(pointSubdivisionFactor / 2)) /* this seems about right */
{
    for (reduceOrder = 1; (1u << reduceOrder) < tDim; ++reduceOrder);

    /* The maximum size of a feature is equal to the cell size
     * divided by the feature subdivision factor.  Like that, I
     * shouldn't get humongous feature pop-in when changing LoDs:
     * all features are minimally visible at greatest zoom.
     * The feature subdivision factor should be something like the
     * point subdivision factor for the local tile (which isn't
     * necessarily the tile its features are homed to...)
     */
    maxFeatureSize = 1.0f / ((float)pointSubdivisionFactor);

    /* ... and the *minimum* size of a feature is equal
     * to that divided by the cell subdivision factor;
     * features smaller than that should be in subcells
     */
    minFeatureSize = maxFeatureSize / (float)subdivisionFactor;
}

unsigned int AFK_LandscapeSizes::getReduceOrder(void) const
{
    return reduceOrder;
}

float AFK_LandscapeSizes::getMinFeatureSize(void) const
{
    return minFeatureSize;
}

float AFK_LandscapeSizes::getMaxFeatureSize(void) const
{
    return maxFeatureSize;
}

