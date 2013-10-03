/* AFK (c) Alex Holloway 2013 */

#include "def.hpp"
#include "shape_sizes.hpp"


AFK_ShapeSizes::AFK_ShapeSizes(
    const AFK_Config *config):
    subdivisionFactor(config->subdivisionFactor),
    entitySubdivisionFactor(config->entitySubdivisionFactor),
    pointSubdivisionFactor(config->shape_pointSubdivisionFactor),
    vDim(config->shape_pointSubdivisionFactor),
    eDim(config->shape_pointSubdivisionFactor + 1), /* one extra vertex along the top and right sides to join with the adjacent tile */
    tDim(config->shape_pointSubdivisionFactor + 3),
    iDim(config->shape_pointSubdivisionFactor),
    /* featureCountPerCube(CUBE(config->shape_pointSubdivisionFactor / 2) / 2), */
    featureCountPerCube(1),
    skeletonMaxSize(config->shape_skeletonMaxSize * 2),
    skeletonFlagGridDim(config->shape_skeletonFlagGridDim),
    skeletonBushiness(1.0f / 4.0f),
    edgeThreshold(config->shape_edgeThreshold)
{
}

