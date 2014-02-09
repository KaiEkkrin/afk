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

#include "afk.hpp"

#include <cfloat>
#include <memory>

#include "core.hpp"
#include "debug.hpp"
#include "object.hpp"
#include "rng/boost_taus88.hpp"
#include "shape_cell.hpp"


/* AFK_ShapeCell implementation. */

Vec4<float> AFK_ShapeCell::getBaseColour(int64_t key) const
{
    /* I think it's okay to recompute this, it's pretty quick. */
    AFK_Boost_Taus88_RNG rng;

    AFK_RNG_Value shapeSeed(
        afk_core.settings.masterSeedLow,
        afk_core.settings.masterSeedHigh,
        key * 0x0013001300130013ll);
    rng.seed(shapeSeed);
    return afk_vec4<float>(
        rng.frand(), rng.frand(), rng.frand(), 0.0f);
}

AFK_ShapeCell::AFK_ShapeCell():
    minDensity(-FLT_MAX), maxDensity(FLT_MAX)
{
}

bool AFK_ShapeCell::hasVapour(AFK_JigsawCollection *vapourJigsaws) const
{
    if (!vapourJigsawPiece) return false;
    AFK_Jigsaw *jigsaw = vapourJigsaws->getPuzzle(vapourJigsawPiece);
    return jigsaw->getTimestamp(vapourJigsawPiece) == vapourJigsawPieceTimestamp;
}

#define SHAPE_COMPUTE_DEBUG 0

void AFK_ShapeCell::enqueueVapourComputeUnitWithNewVapour(
    unsigned int threadId,
    int adjacency,
    const AFK_3DList& list,
    const AFK_KeyedCell& cell,
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair,
    unsigned int& o_cubeOffset,
    unsigned int& o_cubeCount)
{
    vapourJigsaws->grab(0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    /* There's only ever one update queue and one draw queue;
     * all vapours are computed by the same kernel, so that
     * I don't need to worry about cross-vapour stuff.
     */
    std::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(0);

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Shape cell : Computing new vapour: vapour jigsaw piece " << vapourJigsawPiece);
#endif

    vapourComputeQueue->extend(list, o_cubeOffset, o_cubeCount);
    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        getBaseColour(cell.key),
        vapourJigsawPiece,
        adjacency,
        o_cubeOffset,
        o_cubeCount,
        cell);
}

void AFK_ShapeCell::enqueueVapourComputeUnitFromExistingVapour(
    unsigned int threadId,
    int adjacency,
    unsigned int cubeOffset,
    unsigned int cubeCount,
    const AFK_KeyedCell& cell,
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair)
{
    vapourJigsaws->grab(0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    std::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(0);

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Shape cell : Computing existing vapour with cube offset " << cubeOffset << ", cube count " << cubeCount << ": vapour jigsaw piece " << vapourJigsawPiece);
#endif

    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        getBaseColour(cell.key),
        vapourJigsawPiece,
        adjacency,
        cubeOffset,
        cubeCount,
        cell);
}

void AFK_ShapeCell::setDMinMax(float _minDensity, float _maxDensity)
{
    minDensity = _minDensity;
    maxDensity = _maxDensity;
}

void AFK_ShapeCell::evict(void)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell)
{
    os << "Shape cell";
    os << " (Vapour piece: " << shapeCell.vapourJigsawPiece << ")";
    return os;
}

