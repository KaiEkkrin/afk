/* AFK (c) Alex Holloway 2013 */

#include "def.hpp"
#include "landscape_sizes.hpp"


AFK_LandscapeSizes::AFK_LandscapeSizes(unsigned int subdivisionFactor, unsigned int pointSubdivisionFactor):
    subdivisionFactor(subdivisionFactor),
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
}

unsigned int AFK_LandscapeSizes::getReduceOrder(void) const
{
    return reduceOrder;
}

