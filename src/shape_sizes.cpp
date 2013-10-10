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
    featureCountPerCube(CUBE(config->shape_pointSubdivisionFactor) / 2),
    //featureCountPerCube(1),
    skeletonMaxSize(config->shape_skeletonMaxSize),
    skeletonFlagGridDim(config->shape_skeletonFlagGridDim),
    skeletonBushiness(1.0f / 4.0f),
    featureMaxSize(1.0f / (2.0f * (float)config->shape_skeletonFlagGridDim)),
    featureMinSize(1.0f / (2.0f * (float)config->shape_skeletonFlagGridDim * (float)config->subdivisionFactor)),
    edgeThreshold(config->shape_edgeThreshold)
{
}

