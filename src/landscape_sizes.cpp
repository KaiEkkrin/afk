/* AFK (c) Alex Holloway 2013 */

#include "def.hpp"
#include "landscape_sizes.hpp"


AFK_LandscapeSizes::AFK_LandscapeSizes(const AFK_Config *config):
    subdivisionFactor(config->subdivisionFactor),
    pointSubdivisionFactor(config->terrain_pointSubdivisionFactor),
    vDim(config->terrain_pointSubdivisionFactor + 1), /* one extra vertex along the top and right sides to join with the adjacent tile */
    iDim(config->terrain_pointSubdivisionFactor),
    tDim(config->terrain_pointSubdivisionFactor + 3), /* one extra vertex either side, to smooth colours and normals; */
                                      /* another extra vertex on +x and +z, to smooth the join-triangle normals */
    tDimStart(-1),                    /* begin one index before the origin to do the bottom and left smoothing */
    vSize(SQUARE(config->terrain_pointSubdivisionFactor + 1) * sizeof(Vec4<float>)),
    iSize(SQUARE(config->terrain_pointSubdivisionFactor) * 2 * 3 * sizeof(unsigned short)),
    tSize(SQUARE(config->terrain_pointSubdivisionFactor + 3) * 2 * sizeof(Vec4<float>)), /* normal + y disp., colour */
    featureCountPerTile(SQUARE(config->terrain_pointSubdivisionFactor / 2)) /* this seems about right */
{
    for (reduceOrder = 1; (1u << reduceOrder) < tDim; ++reduceOrder);
}

unsigned int AFK_LandscapeSizes::getReduceOrder(void) const
{
    return reduceOrder;
}

