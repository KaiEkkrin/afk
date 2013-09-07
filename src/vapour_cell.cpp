/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "rng/boost_taus88.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"

/* This number defines how much larger than the shape cells
 * the vapour cells shall be.
 */
#define VAPOUR_CELL_MULTIPLIER 8LL

AFK_Cell afk_vapourCell(const AFK_Cell& cell)
{
    return afk_cell(afk_vec4<long long>(
        cell.coord.v[0] * VAPOUR_CELL_MULTIPLIER,
        cell.coord.v[1] * VAPOUR_CELL_MULTIPLIER,
        cell.coord.v[2] * VAPOUR_CELL_MULTIPLIER,
        cell.coord.v[3] * VAPOUR_CELL_MULTIPLIER));
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

        AFK_3DVapourCube cube;
        cube.make(
            features,
            cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
            sSizes,
            rng);
        cubes.push_back(cube);

        haveDescriptor = true;
    }
}

void AFK_VapourCell::build3DList(
    unsigned int threadId,
    AFK_3DList& list,
    unsigned int subdivisionFactor,
    const AFK_VAPOUR_CELL_CACHE *cache)
{
    /* Add the local vapour to the list. */
    list.extend(features, cubes);

    /* If this isn't the top level cell... */
    if (cell.coord.v[3] < (VAPOUR_CELL_MULTIPLIER * SHAPE_CELL_MAX_DISTANCE))
    {
        /* Pull the parent cell from the cache, and
         * include its list too
         */
        AFK_Cell parentCell = cell.parent(subdivisionFactor);
        AFK_VapourCell& parentVapourCell = cache->at(parentCell);
        enum AFK_ClaimStatus claimStatus = parentVapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
        if (claimStatus != AFK_CL_CLAIMED_SHARED && claimStatus != AFK_CL_CLAIMED_UPGRADABLE)
        {
            std::ostringstream ss;
            ss << "Unable to claim VapourCell at " << parentCell << ": got status " << claimStatus;
            throw AFK_Exception(ss.str());
        }
        parentVapourCell.build3DList(threadId, list, subdivisionFactor, cache);
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

