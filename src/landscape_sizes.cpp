/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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
#include "landscape_sizes.hpp"


AFK_LandscapeSizes::AFK_LandscapeSizes(const AFK_ConfigSettings& settings):
    subdivisionFactor(settings.subdivisionFactor),
    pointSubdivisionFactor(afk_terrainPointSubdivisionFactor),
    vDim(afk_terrainPointSubdivisionFactor + 1), /* one extra vertex along the top and right sides to join with the adjacent tile */
    iDim(afk_terrainPointSubdivisionFactor),
    tDim(afk_terrainPointSubdivisionFactor + 3), /* one extra vertex either side, to smooth colours and normals; */
                                      /* another extra vertex on +x and +z, to smooth the join-triangle normals */
    tDimStart(-1),                    /* begin one index before the origin to do the bottom and left smoothing */
    vSize(SQUARE(afk_terrainPointSubdivisionFactor + 1) * sizeof(Vec4<float>)),
    iSize(SQUARE(afk_terrainPointSubdivisionFactor) * 2 * 3 * sizeof(unsigned short)),
    tSize(SQUARE(afk_terrainPointSubdivisionFactor + 3) * 2 * sizeof(Vec4<float>)), /* normal + y disp., colour */
    featureCountPerTile(afk_terrainFeatureCountPerTile) /* this seems about right */
{
    for (reduceOrder = 1; (1u << reduceOrder) < tDim; ++reduceOrder);
}

