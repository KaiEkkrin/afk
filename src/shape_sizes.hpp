/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_SIZES_H_
#define _AFK_SHAPE_SIZES_H_

#include "config.hpp"

/* This utility returns the sizes of various parts of the
 * shape constructs.
 * It's rather similar to LandscapeSizes, but not the same...
 */
class AFK_ShapeSizes
{
public:
    const unsigned int subdivisionFactor; /* same as in landscape_sizes and the world ? */
    const unsigned int entitySubdivisionFactor; /* ratio of cell size to the size of an
                                                 * entity that fits into that cell
                                                 */
    const unsigned int pointSubdivisionFactor;
    const unsigned int vDim; /* Number of vertices along one dimension of each notional distinct cell */
    const unsigned int eDim; /* Number of vertices along one edge of the base face (includes enough to join up to the next face) */
    const unsigned int tDim; /* Number of vertices along one dimension of the vapour
                              * (includes one cell's worth of overlap on the - sides and 2 on the + sides) */
    const unsigned int iDim; /* Number of triangles along an edge */
    const unsigned int iCount; /* Total number of index structures */
    const unsigned int featureCountPerCube; /* Number of features per cube */
    const unsigned int skeletonMaxSize; /* Maximum number of points in a skeleton */
    const unsigned int skeletonFlagGridDim; /* Dimensions of the skeleton flag-grid */
    const float skeletonBushiness; /* Chance of each cube in a skeleton being host to an adjacent one */
    const float edgeThreshold; /* The vapour number that needs to be hit to be called an edge */

    AFK_ShapeSizes(const AFK_Config *config);
};

#endif /* _AFK_SHAPE_SIZES_H_ */

