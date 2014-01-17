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

#ifndef _AFK_LANDSCAPE_SIZES_H_
#define _AFK_LANDSCAPE_SIZES_H_

#include "config.hpp"

/* Some of these will be hardwired, to support fixed-size structures
 * within the code:
 */
const unsigned int afk_terrainPointSubdivisionFactor        = 8;
const unsigned int afk_terrainFeatureCountPerTile           = SQUARE(afk_terrainPointSubdivisionFactor) / 2;
const unsigned int afk_terrainTilesPerTile                  = 5; /* Describes the terrain tile and the 4 half-tiles */

/* This utility returns the sizes of the various landscape
 * elements, so that I can configure the cache correctly, and correctly
 * drive the drawing functions.
 */
class AFK_LandscapeSizes
{
protected:
    unsigned int reduceOrder; /* 1<<reduceOrder is the power of two above tDim */

public:
    const unsigned int subdivisionFactor;
    const unsigned int pointSubdivisionFactor;
    const unsigned int vDim; /* Number of vertices along an edge in the base tile */
    const unsigned int iDim; /* Number of triangles along an edge */
    const unsigned int tDim; /* Number of vertices along an edge in a jigsaw piece */
    const int          tDimStart; /* What index to start the jigsaw pieces at in relation to the tile */
    const unsigned int vSize; /* Size of base tile vertex array in bytes */
    const unsigned int iSize; /* Size of index array in bytes */
    const unsigned int tSize; /* Size of a jigsaw piece in bytes */
    const unsigned int featureCountPerTile; /* Number of terrain features per tile */

    AFK_LandscapeSizes(const AFK_Config *config);

    unsigned int getReduceOrder(void) const { return reduceOrder; }
};

#endif /* _AFK_LANDSCAPE_SIZES_H_ */

