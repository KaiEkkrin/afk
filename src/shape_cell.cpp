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

bool AFK_ShapeCell::hasVapour(const AFK_JigsawCollection *vapourJigsaws) const
{
    return (vapourJigsawPiece != AFK_JigsawPiece() &&
        vapourJigsaws->getPuzzle(vapourJigsawPiece)->getTimestamp(vapourJigsawPiece) == vapourJigsawPieceTimestamp);
}

bool AFK_ShapeCell::hasEdges(const AFK_JigsawCollection *edgeJigsaws) const
{
    return (edgeJigsawPiece != AFK_JigsawPiece() &&
        edgeJigsaws->getPuzzle(edgeJigsawPiece)->getTimestamp(edgeJigsawPiece) == edgeJigsawPieceTimestamp);
}

#define SHAPE_COMPUTE_DEBUG 0

void AFK_ShapeCell::enqueueVapourComputeUnitWithNewVapour(
    unsigned int threadId,
    const AFK_3DList& list,
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair,
    unsigned int& o_cubeOffset,
    unsigned int& o_cubeCount)
{
    vapourJigsaws->grab(threadId, 0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(vapourJigsawPiece.puzzle);

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Computing vapour at location: " << cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE) << " with list: " << list)
#endif

    vapourComputeQueue->extend(list, o_cubeOffset, o_cubeCount);
    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        vapourJigsawPiece,
        o_cubeOffset,
        o_cubeCount);
}

void AFK_ShapeCell::enqueueVapourComputeUnitFromExistingVapour(
    unsigned int threadId,
    unsigned int cubeOffset,
    unsigned int cubeCount,
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair)
{
    vapourJigsaws->grab(threadId, 0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(vapourJigsawPiece.puzzle);

    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        vapourJigsawPiece,
        cubeOffset,
        cubeCount);
}

void AFK_ShapeCell::enqueueEdgeComputeUnit(
    unsigned int threadId,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair)
{
    edgeJigsaws->grab(threadId, 0, &edgeJigsawPiece, &edgeJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DEdgeComputeQueue> edgeComputeQueue =
        edgeComputeFair.getUpdateQueue(
            afk_combineTwoPuzzleFairQueue(vapourJigsawPiece.puzzle, edgeJigsawPiece.puzzle));

#if SHAPE_COMPUTE_DEBUG
    AFK_DEBUG_PRINTL("Computing edges at location: " << cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE))
#endif

    edgeComputeQueue->append(cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        vapourJigsawPiece, edgeJigsawPiece);
}

#define SHAPE_DISPLAY_DEBUG 0

void AFK_ShapeCell::enqueueEdgeDisplayUnit(
    const Mat4<float>& worldTransform,
    const AFK_JigsawCollection *edgeJigsaws,
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

