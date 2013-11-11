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


/* Disable switch for the layers system, which will hopefully plug
 * image gaps, but in its incomplete state it doesn't --
 * with this, the compiled GPU code shouldn't be significantly more
 * heavyweight than the old layer-less code
 */
#define AFK_SHAPE_LAYERS 1


#include <cassert>

#include "def.hpp"
#include "shape_sizes.hpp"


AFK_ShapeSizes::AFK_ShapeSizes(
    const AFK_Config *config):
    subdivisionFactor(config->subdivisionFactor),
    entitySubdivisionFactor(config->entitySubdivisionFactor),
    pointSubdivisionFactor(afk_shapePointSubdivisionFactor),
    vDim(afk_shapePointSubdivisionFactor),
    eDim(afk_shapePointSubdivisionFactor + 1), /* one extra vertex along the top and right sides to join with the adjacent tile */
    tDim(afk_shapePointSubdivisionFactor + 3),
    iDim(afk_shapePointSubdivisionFactor),
    featureCountPerCube(afk_shapeFeatureCountPerCube),
    skeletonMaxSize(config->shape_skeletonMaxSize),
    skeletonFlagGridDim(afk_shapeSkeletonFlagGridDim),
    skeletonBushiness(1.0f / 4.0f),
    featureMaxSize(1.0f / (2.0f * (float)afk_shapeSkeletonFlagGridDim)),
    featureMinSize(1.0f / (2.0f * (float)afk_shapeSkeletonFlagGridDim * (float)config->subdivisionFactor)),
    edgeThreshold(config->shape_edgeThreshold)
{
    for (layerBitness = 3; /* minimum bits for expressing overlap */
        (1u<<layerBitness) < (pointSubdivisionFactor+1);
        ++layerBitness);

    assert(layerBitness < (8 * sizeof(uint32_t)));

#if AFK_SHAPE_LAYERS
    /* limiting layers to pointSubdivisionFactor isn't strictly necessary
     * for the algorithm, but having more layers than that would be,
     * you know, silly, because it wouldn't be possible to
     * actually populate them all :P
     */
    for (layers = 0;
        (layers + 1) * layerBitness < (8 * sizeof(uint32_t)) && (layers + 1) < pointSubdivisionFactor;
        ++layers);
#else
    layers = 1;
#endif
}

