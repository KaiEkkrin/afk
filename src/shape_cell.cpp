/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/functional/hash.hpp>

#include "core.hpp"
#include "object.hpp"
#include "rng/boost_taus88.hpp"
#include "shape_cell.hpp"


/* AFK_ShapeCell implementation. */

Mat4<float> AFK_ShapeCell::getCellToShapeTransform(void) const
{
    float shapeScale = (float)cell.coord.v[3] / (float)SHAPE_CELL_MAX_DISTANCE;
    Vec3<float> shapeLocation = afk_vec3<float>(
        (float)cell.coord.v[0],
        (float)cell.coord.v[1],
        (float)cell.coord.v[2]);

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

bool AFK_ShapeCell::hasVapourDescriptor(void) const
{
    return haveVapourDescriptor;
}

void AFK_ShapeCell::makeVapourDescriptor(
    unsigned int shapeKey,
    const AFK_ShapeSizes& sSizes)
{
    if (!haveVapourDescriptor)
    {
        AFK_Boost_Taus88_RNG rng;

        /* TODO: Half-cells will go here when I add them (which I
         * think I might want to).  For now I just make this cell
         * by itself.
         */
        rng.seed(cell.rngSeed(
            0x0001000100010001LL * shapeKey));

        AFK_3DVapourCube cube;
        cube.make(
            vapourFeatures,
            cell.toWorldSpace(1.0f), /* TODO ??? */
            sSizes,
            rng);
        vapourCubes.push_back(cube);

        haveVapourDescriptor = true;
    }
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

void AFK_ShapeCell::build3DList(
    unsigned int threadId,
    AFK_3DList& list,
    unsigned int subdivisionFactor,
    const AFK_SHAPE_CELL_CACHE *cache)
{
    /* Add the local vapour to the list. */
    list.extend(vapourFeatures, vapourCubes);

    /* If this isn't the top level cell... */
    if (cell.coord.v[3] < SHAPE_CELL_MAX_DISTANCE)
    {
        /* Pull the parent cell from the cache, and
         * include its list too
         */
        AFK_Cell parentCell = cell.parent(subdivisionFactor);
        AFK_ShapeCell& parentShapeCell = cache->at(parentCell);
        enum AFK_ClaimStatus claimStatus = parentShapeCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
        if (claimStatus != AFK_CL_CLAIMED_SHARED && claimStatus != AFK_CL_CLAIMED_UPGRADABLE)
        {
            std::ostringstream ss;
            ss << "Unable to claim ShapeCell at " << parentCell << ": got status " << claimStatus;
            throw AFK_Exception(ss.str());
        }
        parentShapeCell.build3DList(threadId, list, subdivisionFactor, cache);
        parentShapeCell.release(threadId, claimStatus);
    }
}

void AFK_ShapeCell::enqueueVapourComputeUnit(
    unsigned int threadId,
    const AFK_3DList& list,
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair)
{
    vapourJigsaws->grab(threadId, 0, &vapourJigsawPiece, &vapourJigsawPieceTimestamp, 1);

    boost::shared_ptr<AFK_3DVapourComputeQueue> vapourComputeQueue =
        vapourComputeFair.getUpdateQueue(vapourJigsawPiece.puzzle);

    vapourComputeQueue->extend(list, cell.toWorldSpace(1.0f), vapourJigsawPiece, sSizes);
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

    edgeComputeQueue->append(cell.toWorldSpace(1.0f), vapourJigsawPiece, edgeJigsawPiece);
}

void AFK_ShapeCell::enqueueEdgeDisplayUnit(
    const Mat4<float>& worldTransform,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const
{
    /* The supplied world transform is for the shape -- to get
     * an overall world transform for this specific cell I'll
     * need to make a cell to shape transform and concatenate
     * them :
     */
    Mat4<float> cellToShapeTransform = getCellToShapeTransform();
    Mat4<float> cellToWorldTransform = worldTransform * cellToShapeTransform;

    entityDisplayFair.getUpdateQueue(edgeJigsawPiece.puzzle)->add(
        AFK_EntityDisplayUnit(
            cellToWorldTransform,
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
    if (shapeCell.haveVapourDescriptor) os << " (Vapour)";
    os << " (Vapour piece: " << shapeCell.vapourJigsawPiece << ")";
    os << " (Edge piece: " << shapeCell.edgeJigsawPiece << ")";
    return os;
}

