/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/functional/hash.hpp>

#include "core.hpp"
#include "debug.hpp"
#include "object.hpp"
#include "shape_cell.hpp"


/* AFK_ShapeCell implementation. */

void AFK_ShapeCell::bind(const AFK_KeyedCell& _cell)
{
    cell = _cell;
}

const AFK_KeyedCell& AFK_ShapeCell::getCell(void) const
{
    return cell;
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
    AFK_DEBUG_PRINTL("Computing new vapour at location: " << cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE) << " with adjacency: " << std::hex << adjacency)
#endif

    vapourComputeQueue->extend(list, o_cubeOffset, o_cubeCount);
    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
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
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair)
{
    vapourJigsaws->grab(threadId, 0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(0);

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Computing existing vapour at location: " << cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE) << " with adjacency: " << std::hex << adjacency)
#endif

    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        vapourJigsawPiece,
        adjacency,
        cubeOffset,
        cubeCount);
}

void AFK_ShapeCell::enqueueEdgeComputeUnit(
    unsigned int threadId,
    const AFK_SHAPE_CELL_CACHE *cache,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair)
{
    edgeJigsaws->grab(threadId, 0, &edgeJigsawPiece, &edgeJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DEdgeComputeQueue> edgeComputeQueue =
        edgeComputeFair.getUpdateQueue(edgeJigsawPiece.puzzle);

#if SHAPE_COMPUTE_DEBUG
     AFK_DEBUG_PRINTL("Computing edges at location: " << cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE))
#endif

     edgeComputeQueue->append(cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
         vapourJigsawPiece, edgeJigsawPiece);
}

#define SHAPE_DISPLAY_DEBUG 0

void AFK_ShapeCell::enqueueEdgeDisplayUnit(
    const Mat4<float>& worldTransform,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const
{
#if SHAPE_DISPLAY_DEBUG
    AFK_DEBUG_PRINTL("Displaying edges at cell " << worldTransform * cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE))
#endif

    entityDisplayFair.getUpdateQueue(edgeJigsawPiece.puzzle)->add(
        AFK_EntityDisplayUnit(
            worldTransform,
            edgeJigsaws->getPuzzle(edgeJigsawPiece)->getTexCoordST(edgeJigsawPiece)));
}

bool AFK_ShapeCell::canBeEvicted(void) const
{
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell)
{
    os << "Shape cell at " << shapeCell.cell;
    os << " (Vapour piece: " << shapeCell.vapourJigsawPiece << ")";
    os << " (Edge piece: " << shapeCell.edgeJigsawPiece << ")";
    return os;
}

