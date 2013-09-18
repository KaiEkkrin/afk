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

bool AFK_ShapeCell::getVapourJigsawPiece(
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawPiece *o_jigsawPiece) const
{
    if (hasVapour(vapourJigsaws))
    {
        *o_jigsawPiece = vapourJigsawPiece;
        return true;
    }
    else return false;
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

    /* There's only ever one update queue and one draw queue;
     * all vapours are computed by the same kernel, so that
     * I don't need to worry about cross-vapour stuff.
     */
    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(0);

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
        vapourComputeFair.getUpdateQueue(0);

    vapourComputeQueue->addUnit(
        cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        vapourJigsawPiece,
        cubeOffset,
        cubeCount);
}

/* Helper for the below. */
static void getVapourJigsawPieceAt(
    unsigned int threadId,
    const AFK_KeyedCell& cell,
    const AFK_SHAPE_CELL_CACHE *cache,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawPiece *o_jigsawPiece)
{
    bool gotPiece = false;

    try
    {
        AFK_ShapeCell& shapeCell = cache->at(cell);
        AFK_ClaimStatus claimStatus = shapeCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);
        gotPiece = shapeCell.getVapourJigsawPiece(vapourJigsaws, o_jigsawPiece);
        shapeCell.release(threadId, claimStatus);
    }
    catch (AFK_PolymerOutOfRange) {}

    if (!gotPiece)
    {
        /* It's normal to be missing some adjacent cells, e.g.
         * when they're outside the skeleton.
         * I'll provide this special jigsaw piece instead:
         * the OpenCL will use default zeroes when it finds
         * a skeleton-edge adjacency piece like this.
         */
        *o_jigsawPiece = AFK_JigsawPiece(0, 0, 0, -1);
    }
}

void AFK_ShapeCell::enqueueEdgeComputeUnit(
    unsigned int threadId,
    const AFK_SHAPE_CELL_CACHE *cache,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair)
{
    /* First of all, build the vapour adjacency list to make sure
     * that I have everything I need.
     * TODO I suspect that this is a very bad idea and I should
     * move to half-cubes instead, like the landscape: doing this
     * means I'm introducing a vapour cell cross-dependency.
     * However, by substituting zero cells for unfulfilled
     * dependencies rather than enqueueing a dependent task and
     * resuming I shouldn't be causing deadlocks, just getting
     * spurious black borders appearing in the middle of pieces.
     * We'll see.
     * - TODO *2: okay, actually it deadlocks right away, as
     * all the threads pile into trying to lock cells for
     * adjacencies all the while holding exclusive locks for
     * adjacent cells in order to build their contents.
     * Maybe I should throw adjacency away, give in, and
     * compute the adjacent vertices in each vapour cube after
     * all?  Even though it triples the size of each cube at
     * the small sizes I'm working with?  :(
     */
    AFK_Cell adjacentCells[7];
    AFK_JigsawPiece vapourAdjacency[7];
    adjacentCells[0] = cell.c;
    cell.c.faceAdjacency(&adjacentCells[1], 6);

    vapourAdjacency[0] = vapourJigsawPiece;
    for (int i = 1; i < 7; ++i)
    {
        AFK_KeyedCell thisCell = afk_keyedCell(adjacentCells[i], cell.key);
        getVapourJigsawPieceAt(threadId, thisCell, cache, vapourJigsaws, &vapourAdjacency[i]);
    }

    edgeJigsaws->grab(threadId, 0, &edgeJigsawPiece, &edgeJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DEdgeComputeQueue> edgeComputeQueue =
        edgeComputeFair.getUpdateQueue(edgeJigsawPiece.puzzle);

#if SHAPE_COMPUTE_DEBUG
     AFK_DEBUG_PRINTL("Computing edges at location: " << cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE))
#endif

     edgeComputeQueue->append(cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
         vapourAdjacency, edgeJigsawPiece);
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

