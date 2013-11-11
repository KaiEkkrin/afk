/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_SHAPE_SIZES_H_
#define _AFK_SHAPE_SIZES_H_

#include "config.hpp"
#include "def.hpp"

/* Hardwired ones to support fixed-size structures in the code: */
const unsigned int afk_shapePointSubdivisionFactor      = 6;
const unsigned int afk_shapeFeatureCountPerCube         = CUBE(afk_shapePointSubdivisionFactor) / 2;
const unsigned int afk_shapeSkeletonFlagGridDim         = 8;

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
    const unsigned int featureCountPerCube; /* Number of features per cube */
    const unsigned int skeletonMaxSize; /* Maximum number of points in a skeleton */
    const unsigned int skeletonFlagGridDim; /* Dimensions of the skeleton flag-grid */
    const float skeletonBushiness; /* Chance of each cube in a skeleton being host to an adjacent one */
    const float featureMaxSize; /* Max feature size in vapour cube space */
    const float featureMinSize; /* Min feature size in vapour cube space */
    const float edgeThreshold; /* The vapour number that needs to be hit to be called an edge */
    unsigned int layerBitness; /* 1<<layerBitness is the next power of 2 above pointSubdivisionFactor+1.
                                * This value determines the number of bits that each entry in the overlap
                                * texture occupies.  The +1 is there to reserve the value 0 to mean "no edge";
                                * the actual edgeStepsBack value should be decremented by 1 when used.
                                */
    unsigned int layers;        /* The number of layers per edge, i.e. the number of entries (of
                                 * layerBitness size each) packed into the channels of the overlap texels.
                                 */

    AFK_ShapeSizes(const AFK_Config *config);
};

#endif /* _AFK_SHAPE_SIZES_H_ */

