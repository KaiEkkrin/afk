/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "rng/boost_taus88.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"


AFK_Cell afk_shapeToVapourCell(const AFK_Cell& cell, const AFK_ShapeSizes& sSizes)
{
    return afk_cell(cell.coord * (long long)sSizes.skeletonFlagGridDim);
}

AFK_Cell afk_vapourToShapeCell(const AFK_Cell& cell, const AFK_ShapeSizes& sSizes)
{
    return afk_cell(cell.coord / (long long)sSizes.skeletonFlagGridDim);
}


/* AFK_VapourCell implementation. */

void AFK_VapourCell::bind(const AFK_Cell& _cell)
{
    cell = _cell;
}

bool AFK_VapourCell::hasDescriptor(void) const
{
    return haveDescriptor;
}

void AFK_VapourCell::makeDescriptor(
    unsigned int shapeKey,
    const AFK_ShapeSizes& sSizes)
{
    if (!haveDescriptor)
    {
        AFK_Boost_Taus88_RNG rng;

        /* TODO: Half-cells will go here when I add them (which I
         * think I might want to).  For now I just make this cell
         * by itself.
         */
        rng.seed(cell.rngSeed(
            0x0001000100010001LL * shapeKey));

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
    unsigned int shapeKey,
    const AFK_VapourCell& upperCell,
    const AFK_ShapeSizes& sSizes)
{
    /* Sanity check. */
    if (!upperCell.hasDescriptor()) throw AFK_Exception("Vapour cell descriptors built in wrong order");

    if (!haveDescriptor)
    {
        AFK_Boost_Taus88_RNG rng;

        /* TODO: Half-cells will go here when I add them (which I
         * think I might want to).  For now I just make this cell
         * by itself.
         */
        rng.seed(cell.rngSeed(
            0x0001000100010001LL * shapeKey));

        Vec3<long long> thisCellShapeSpace = afk_vec3<long long>(
            cell.coord.v[0], cell.coord.v[1], cell.coord.v[2]);
        Vec3<long long> upperCellShapeSpace = afk_vec3<long long>(
            upperCell.cell.coord.v[0], upperCell.cell.coord.v[1], upperCell.cell.coord.v[2]);
        Vec3<long long> upperOffset = (upperCellShapeSpace - thisCellShapeSpace) / cell.coord.v[3];

        /* TODO: I'm seeing co-ordinates of -1 in the upper offset.
         * That's almost certainly wrong.  I need to understand what
         * makes them and fix them.
         */
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

bool AFK_VapourCell::withinSkeleton(void) const
{
    return (skeleton.getBoneCount() > 0);
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

AFK_Cell AFK_VapourCell::ShapeCells::next(void)
{
    AFK_SkeletonCube nextSkeletonCube = bones.next();
    long long shapeCellScale = vapourCell.cell.coord.v[3] / sSizes.skeletonFlagGridDim;
    return afk_cell(afk_vec4<long long>(
        nextSkeletonCube.coord.v[0] * shapeCellScale + vapourCell.cell.coord.v[0],
        nextSkeletonCube.coord.v[1] * shapeCellScale + vapourCell.cell.coord.v[1],
        nextSkeletonCube.coord.v[2] * shapeCellScale + vapourCell.cell.coord.v[2],
        shapeCellScale));
}

void AFK_VapourCell::build3DList(
    unsigned int threadId,
    AFK_3DList& list,
    std::vector<AFK_Cell>& missingCells,
    const AFK_ShapeSizes& sSizes,
    const AFK_VAPOUR_CELL_CACHE *cache)
{
    /* Add the local vapour to the list. */
    list.extend(features, cubes);

    AFK_Cell currentCell = cell;

    /* If this isn't the top level cell... */
    while (currentCell.coord.v[3] < (sSizes.skeletonFlagGridDim * SHAPE_CELL_MAX_DISTANCE))
    {
        /* Pull the parent cell from the cache, and
         * include its list too
         */
        AFK_Cell parentCell = currentCell.parent(sSizes.subdivisionFactor);

        try
        {
            AFK_VapourCell& parentVapourCell = cache->at(parentCell);
            enum AFK_ClaimStatus claimStatus = parentVapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
            if (claimStatus != AFK_CL_CLAIMED_SHARED && claimStatus != AFK_CL_CLAIMED_UPGRADABLE)
            {
                std::ostringstream ss;
                ss << "Unable to claim VapourCell at " << parentCell << ": got status " << claimStatus;
                throw AFK_Exception(ss.str());
            }
            parentVapourCell.build3DList(threadId, list, missingCells, sSizes, cache);
            parentVapourCell.release(threadId, claimStatus);

            /* At this point we've actually finished, the recursive
             * call did the rest
             */
            return;
        }
        catch (AFK_PolymerOutOfRange)
        {
            /* Oh dear, we're going to need to recompute this one */
            missingCells.push_back(afk_vapourToShapeCell(parentCell, sSizes));

            /* Keep looking for other cells we might be missing so
             * we can do all that at once
             */
            currentCell = parentCell;
        }
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

AFK_Frame AFK_VapourCell::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
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

