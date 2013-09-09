/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/functional/hash.hpp>

#include "core.hpp"
#include "object.hpp"
#include "shape_cell.hpp"


/* AFK_ShapeCell implementation. */

Mat4<float> AFK_ShapeCell::getCellToShapeTransform(void) const
{
    float shapeScale = (float)cell.coord.v[3] * SHAPE_CELL_WORLD_SCALE;
    Vec3<float> shapeLocation = afk_vec3<float>(
        (float)cell.coord.v[0] * SHAPE_CELL_WORLD_SCALE,
        (float)cell.coord.v[1] * SHAPE_CELL_WORLD_SCALE,
        (float)cell.coord.v[2] * SHAPE_CELL_WORLD_SCALE);

    AFK_Object cellObj;
    cellObj.resize(afk_vec3<float>(shapeScale, shapeScale, shapeScale));
    cellObj.displace(shapeLocation);
    return cellObj.getTransformation();
}

void AFK_ShapeCell::bind(const AFK_Cell& _cell)
{
    cell = _cell;
}

const AFK_Cell& AFK_ShapeCell::getCell(void) const
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

    edgeComputeQueue->append(cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
        /* TODO REMOVE DEBUG */
        (cell.coord.v[0] != 0 || cell.coord.v[1] != 0  || cell.coord.v[2] != 0) ?
            afk_vec4<float>(1.0f, 1.0f, 0.0f, 0.0f) :
            afk_vec4<float>(0.0f, 0.0f, 1.0f, 0.0f),
        vapourJigsawPiece, edgeJigsawPiece);
}

void AFK_ShapeCell::enqueueEdgeDisplayUnit(
    const Mat4<float>& worldTransform,
    const AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const
{
    /* The supplied world transform is for the shape -- to get
     * an overall world transform for this specific cell I'll
     * need to make a cell to shape transform and concatenate
     * them :
     */
#if 0
    Mat4<float> cellToShapeTransform = getCellToShapeTransform();
    Mat4<float> cellToWorldTransform = worldTransform * cellToShapeTransform;
#endif

    entityDisplayFair.getUpdateQueue(edgeJigsawPiece.puzzle)->add(
        AFK_EntityDisplayUnit(
            /* cellToWorldTransform */ worldTransform,
            edgeJigsaws->getPuzzle(edgeJigsawPiece)->getTexCoordST(edgeJigsawPiece)));
}

AFK_Frame AFK_ShapeCell::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_ShapeCell::canBeEvicted(void) const
{
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell)
{
    os << "Shape cell";
    os << " (Vapour piece: " << shapeCell.vapourJigsawPiece << ")";
    os << " (Edge piece: " << shapeCell.edgeJigsawPiece << ")";
    return os;
}

