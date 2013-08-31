/* AFK (c) Alex Holloway 2013 */

#include "def.hpp"
#include "shape_sizes.hpp"


AFK_ShapeSizes::AFK_ShapeSizes(
    const AFK_Config *config):
    subdivisionFactor(config->subdivisionFactor),
    entitySubdivisionFactor(config->entitySubdivisionFactor),
    pointSubdivisionFactor(config->shape_pointSubdivisionFactor),
    vDim(config->shape_pointSubdivisionFactor + 1), /* one extra vertex along the top and right sides to join with the adjacent tile */
    iDim(config->shape_pointSubdivisionFactor),
    tDim(config->shape_pointSubdivisionFactor + 3), /* one extra vertex either side, to smooth colours and normals; */
                                      /* another extra vertex on +x and +z, to smooth the join-triangle normals */
    tDimStart(-1),                    /* begin one index before the origin to do the bottom and left smoothing */
    vCount(SQUARE(config->shape_pointSubdivisionFactor + 1)),
    iCount(SQUARE(config->shape_pointSubdivisionFactor) * 2), /* two triangles per vertex */
    tCount(SQUARE(config->shape_pointSubdivisionFactor + 3)),
    vSize(SQUARE(config->shape_pointSubdivisionFactor + 1) * sizeof(Vec4<float>)),
    iSize(SQUARE(config->shape_pointSubdivisionFactor) * 2 * 3 * sizeof(unsigned short)),
    tSize(SQUARE(config->shape_pointSubdivisionFactor + 3) * 2 * sizeof(Vec4<float>)), /* normal + y disp., colour */
    featureCountPerCube(CUBE(config->shape_pointSubdivisionFactor / 2)), /* TODO Probably wants experimenting with */
    skeletonMaxSize(config->shape_skeletonMaxSize * 2),
    skeletonFlagGridDim(config->shape_skeletonFlagGridDim),
    skeletonBushiness(1.0f / 4.0f),
    edgeThreshold(config->shape_edgeThreshold)
{
}

