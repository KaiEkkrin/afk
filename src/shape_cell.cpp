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

#include "afk.hpp"

#include <boost/functional/hash.hpp>

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
        afk_core.config->masterSeed.v.ll[0],
        afk_core.config->masterSeed.v.ll[1],
        key * 0x0013001300130013ll);
    rng.seed(shapeSeed);
    return afk_vec4<float>(
        rng.frand(), rng.frand(), rng.frand(), 0.0f);
}

AFK_ShapeCell::AFK_ShapeCell()
{
}

bool AFK_ShapeCell::hasVapour(AFK_JigsawCollection *vapourJigsaws) const
{
    return (vapourJigsawPiece != AFK_JigsawPiece() &&
        vapourJigsaws->getPuzzle(vapourJigsawPiece)->getTimestamp(vapourJigsawPiece) == vapourJigsawPieceTimestamp);
}

bool AFK_ShapeCell::hasEdges(AFK_JigsawCollection *edgeJigsaws) const
{
    return (edgeJigsawPiece != AFK_JigsawPiece() &&
        edgeJigsaws->getPuzzle(edgeJigsawPiece)->getTimestamp(edgeJigsawPiece) == edgeJigsawPieceTimestamp);
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
    vapourJigsaws->grab(threadId, 0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    /* There's only ever one update queue and one draw queue;
     * all vapours are computed by the same kernel, so that
     * I don't need to worry about cross-vapour stuff.
     */
    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(0);

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Shape cell : Computing new vapour: vapour jigsaw piece " << vapourJigsawPiece)
#endif

    vapourComputeQueue->extend(list, o_cubeOffset, o_cubeCount);
    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        getBaseColour(cell.key),
        vapourJigsawPiece,
        adjacency,
        o_cubeOffset,
        o_cubeCount);
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
    vapourJigsaws->grab(threadId, 0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(0);

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Shape cell : Computing existing vapour with cube offset " << cubeOffset << ", cube count " << cubeCount << ": vapour jigsaw piece " << vapourJigsawPiece)
#endif

    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        getBaseColour(cell.key),
        vapourJigsawPiece,
        adjacency,
        cubeOffset,
        cubeCount);
}

void AFK_ShapeCell::enqueueEdgeComputeUnit(
    unsigned int threadId,
    AFK_SHAPE_CELL_CACHE *cache,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair,
    const AFK_Fair2DIndex& entityFair2DIndex)
{
    edgeJigsaws->grab(threadId, 0, &edgeJigsawPiece, &edgeJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DEdgeComputeQueue> edgeComputeQueue =
        edgeComputeFair.getUpdateQueue(
            entityFair2DIndex.get1D(vapourJigsawPiece.puzzle, edgeJigsawPiece.puzzle));

#if SHAPE_COMPUTE_DEBUG
     AFK_DEBUG_PRINTL("Computing edges: vapour jigsaw piece " << vapourJigsawPiece << ", edge jigsaw piece " << edgeJigsawPiece)
#endif

     edgeComputeQueue->append(vapourJigsawPiece, edgeJigsawPiece);
}

#define SHAPE_DISPLAY_DEBUG 0

void AFK_ShapeCell::enqueueEdgeDisplayUnit(
    const Mat4<float>& worldTransform,
    const AFK_KeyedCell& cell,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair,
    const AFK_Fair2DIndex& entityFair2DIndex) const
{
    /* TODO Deal with multiple vapour jigsaws in this queue.
     * I think I'll want to decide I'm fed up with handling
     * vapour0 vapour1 vapour2 vapour3 everywhere, etc,
     * and go back to an interleaved queue (but I should have
     * a better scheme than the current one!)
     */
    unsigned int index = entityFair2DIndex.get1D(vapourJigsawPiece.puzzle, edgeJigsawPiece.puzzle);

    Vec4<float> hgCoord = cell.toHomogeneous(SHAPE_CELL_WORLD_SCALE);
#if SHAPE_DISPLAY_DEBUG
    AFK_DEBUG_PRINTL("Displaying shape at hgCoord " << hgCoord << " with vapour jigsaw piece " << vapourJigsawPiece << ", edge jigsaw piece " << edgeJigsawPiece)
#endif
    entityDisplayFair.getUpdateQueue(index)->add(
        AFK_EntityDisplayUnit(
            worldTransform,
            hgCoord,
            vapourJigsaws->getPuzzle(vapourJigsawPiece)->getTexCoordSTR(vapourJigsawPiece),
            edgeJigsaws->getPuzzle(edgeJigsawPiece)->getTexCoordST(edgeJigsawPiece)));
}

void AFK_ShapeCell::evict(void)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell)
{
    os << "Shape cell";
    os << " (Vapour piece: " << shapeCell.vapourJigsawPiece << ")";
    os << " (Edge piece: " << shapeCell.edgeJigsawPiece << ")";
    return os;
}

