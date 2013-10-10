/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cassert>

#include "core.hpp"
#include "debug.hpp"
#include "rng/boost_taus88.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"


AFK_KeyedCell afk_shapeToVapourCell(const AFK_KeyedCell& cell, const AFK_ShapeSizes& sSizes)
{
    return cell.parent(sSizes.skeletonFlagGridDim);
}


/* AFK_VapourCell implementation. */

void AFK_VapourCell::bind(const AFK_KeyedCell& _cell)
{
    cell = _cell;
}

bool AFK_VapourCell::hasDescriptor(void) const
{
    return haveDescriptor;
}

void AFK_VapourCell::makeDescriptor(
    const AFK_ShapeSizes& sSizes)
{
    if (!haveDescriptor)
    {
        AFK_Boost_Taus88_RNG rng;

        rng.seed(cell.rngSeed());
        skeleton.make(rng, sSizes);

        AFK_3DVapourCube cube;
        cube.make(
            features,
            cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
            skeleton,
            sSizes,
            rng);
        cubes.push_back(cube);

        haveDescriptor = true;
    }
}

void AFK_VapourCell::makeDescriptor(
    const AFK_VapourCell& upperCell,
    const AFK_ShapeSizes& sSizes)
{
    /* Sanity check. */
    if (!upperCell.hasDescriptor()) throw AFK_Exception("Vapour cell descriptors built in wrong order");

    if (!haveDescriptor)
    {
        AFK_Boost_Taus88_RNG rng;

        rng.seed(cell.rngSeed());

        Vec3<int64_t> thisCellShapeSpace = afk_vec3<int64_t>(
            cell.c.coord.v[0], cell.c.coord.v[1], cell.c.coord.v[2]);
        Vec3<int64_t> upperCellShapeSpace = afk_vec3<int64_t>(
            upperCell.cell.c.coord.v[0], upperCell.cell.c.coord.v[1], upperCell.cell.c.coord.v[2]);
        Vec3<int64_t> upperOffset = (thisCellShapeSpace - upperCellShapeSpace) * (sSizes.skeletonFlagGridDim/2) / cell.c.coord.v[3];

        if (skeleton.make(
            upperCell.skeleton,
            upperOffset,
            rng,
            sSizes) > 0)
        {
            AFK_3DVapourCube cube;
            cube.make(
                features,
                cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
                skeleton,
                sSizes,
                rng);
            cubes.push_back(cube);
        }

        haveDescriptor = true;
    }
}

bool AFK_VapourCell::withinSkeleton(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const
{
    assert(shapeCell.c.coord.v[3] == cell.c.coord.v[3] / sSizes.skeletonFlagGridDim);

    AFK_SkeletonCube cube(cell, shapeCell, sSizes);
    return skeleton.within(cube);
}

int AFK_VapourCell::skeletonAdjacency(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const
{
    assert(shapeCell.c.coord.v[3] == cell.c.coord.v[3] / sSizes.skeletonFlagGridDim);

    AFK_SkeletonCube cube(cell, shapeCell, sSizes);
    return skeleton.getAdjacency(cube);
}

int AFK_VapourCell::skeletonFullAdjacency(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const
{
    assert(shapeCell.c.coord.v[3] == cell.c.coord.v[3] / sSizes.skeletonFlagGridDim);

    AFK_SkeletonCube cube(cell, shapeCell, sSizes);
    return skeleton.getFullAdjacency(cube);
}

AFK_VapourCell::ShapeCells::ShapeCells(const AFK_VapourCell& _vapourCell, const AFK_ShapeSizes& _sSizes):
    bones(AFK_Skeleton::Bones(_vapourCell.skeleton)),
    vapourCell(_vapourCell),
    sSizes(_sSizes)
{
}

bool AFK_VapourCell::ShapeCells::hasNext(void)
{
    return bones.hasNext();
}

AFK_KeyedCell AFK_VapourCell::ShapeCells::next(void)
{
    AFK_SkeletonCube nextSkeletonCube = bones.next();
    AFK_KeyedCell nextCell = nextSkeletonCube.toShapeCell(vapourCell.cell, sSizes);

    return nextCell;
}

void AFK_VapourCell::build3DList(
    unsigned int threadId,
    AFK_3DList& list,
    const AFK_ShapeSizes& sSizes,
    const AFK_VAPOUR_CELL_CACHE *cache) const
{
    /* Add the local vapour to the list. */
    list.extend(features, cubes);

    AFK_KeyedCell currentCell = cell;

    /* If this isn't the top level cell... */
    if (currentCell.c.coord.v[3] < (sSizes.skeletonFlagGridDim * SHAPE_CELL_MAX_DISTANCE))
    {
        /* Pull the parent cell from the cache, and
         * include its list too
         */
        AFK_KeyedCell parentCell = currentCell.parent(sSizes.subdivisionFactor);
        AFK_VapourCell& parentVapourCell = cache->at(parentCell);
        enum AFK_ClaimStatus claimStatus = parentVapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);
        if (claimStatus != AFK_CL_CLAIMED_SHARED && claimStatus != AFK_CL_CLAIMED_UPGRADABLE)
        {
            std::ostringstream ss;
            ss << "Unable to claim VapourCell at " << parentCell << ": got status " << claimStatus;
            throw AFK_Exception(ss.str());
        }
        parentVapourCell.build3DList(threadId, list, sSizes, cache);
        parentVapourCell.release(threadId, claimStatus);
    }
}

bool AFK_VapourCell::alreadyEnqueued(
    unsigned int& o_cubeOffset,
    unsigned int& o_cubeCount) const
{
    if (computeCubeFrame == afk_core.computingFrame)
    {
        o_cubeOffset = computeCubeOffset;
        o_cubeCount = computeCubeCount;
        return true;
    }
    else return false;
}

void AFK_VapourCell::enqueued(
    unsigned int cubeOffset,
    unsigned int cubeCount)
{
    /* Sanity check */
    if (computeCubeFrame == afk_core.computingFrame)
        throw AFK_Exception("Vapour cell enqueue conflict");

    computeCubeOffset = cubeOffset;
    computeCubeCount = cubeCount;
    computeCubeFrame = afk_core.computingFrame;
}

bool AFK_VapourCell::canBeEvicted(void) const
{
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell)
{
    os << "Vapour cell with " << vapourCell.features.size() << " features and " << vapourCell.cubes.size() << " cubes";
    return os;
}

