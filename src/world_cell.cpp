/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/memory_order.hpp>

#include "core.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"
#include "world_cell.hpp"


/* This is a special "unclaimed" thread ID value.  It'll
 * never be a real thread ID.
 */
#define UNCLAIMED 0xffffffffu

AFK_WorldCell::AFK_WorldCell():
    claimingThreadId(UNCLAIMED), hasTerrain(false)
{
}

enum AFK_WorldCellClaimStatus AFK_WorldCell::claim(unsigned int threadId, bool touch)
{
    unsigned int expectedId = UNCLAIMED;
    if (claimingThreadId.compare_exchange_strong(expectedId, threadId))
    {
        if (touch && lastSeen == afk_core.computingFrame)
        {
            /* This cell already got processed this frame,
             * it shouldn't get claimed again
             */
            release(threadId);
            return AFK_WCC_ALREADY_PROCESSED;
        }
        else
        {
            if (touch) lastSeen = afk_core.computingFrame;
            return AFK_WCC_CLAIMED;
        }
    }
    else
    {
        if (touch && lastSeen == afk_core.computingFrame) return AFK_WCC_ALREADY_PROCESSED;
        else return AFK_WCC_TAKEN;
    }
}

void AFK_WorldCell::release(unsigned int threadId)
{
    if (!claimingThreadId.compare_exchange_strong(threadId, UNCLAIMED))
        throw AFK_Exception("Concurrency screwup");
}

bool AFK_WorldCell::claimYieldLoop(unsigned int threadId, bool touch)
{
    bool claimed = false;
    for (unsigned int tries = 0; !claimed && tries < 2; ++tries)
    {
        enum AFK_WorldCellClaimStatus status = claim(threadId, touch);
        switch (status)
        {
        case AFK_WCC_CLAIMED:
            claimed = true;
            break;

        case AFK_WCC_ALREADY_PROCESSED:
            return false;

        case AFK_WCC_TAKEN:
            boost::this_thread::yield();
            break;

        default:
            {
                std::ostringstream ss;
                ss << "Unrecognised claim status: " << status;
                throw new AFK_Exception(ss.str());
            }
        }
    }

    return claimed;
}

void AFK_WorldCell::bind(const AFK_Cell& _cell, float _worldScale)
{
    cell = _cell;
    /* TODO If this actually *changes* something, I need to
     * clear the terrain so that it can be re-made.
     */
    realCoord = _cell.toWorldSpace(_worldScale);
}

bool AFK_WorldCell::testDetailPitch(
    float detailPitch,
    const AFK_Camera& camera,
    const Vec3<float>& viewerLocation) const
{
    /* Sample the centre of the cell.  This is wrong: it
     * will cause artifacts if you manage to get to exactly
     * the middle of a cell (I can probably test this with
     * the start position (8192, 8192, 8192)
     * TODO To fix it properly, I need to pick three points
     * displaced along the 3 axes by the dot pitch from the
     * centre of the cell, project them through the camera,
     * and compare those distances to the detail pitch,
     * no?
     * (in fact I'd probably get away with just the x and
     * z axes)
     */
    Vec3<float> centre = afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3] / 2.0f,
        realCoord.v[1] + realCoord.v[3] / 2.0f,
        realCoord.v[2] + realCoord.v[3] / 2.0f);
    Vec3<float> facing = centre - viewerLocation;
    float distanceToViewer = facing.magnitude();

    /* Magic */
    float cellDetailPitch = camera.windowHeight * realCoord.v[3] /
        (camera.tanHalfFov * distanceToViewer);
    
    return cellDetailPitch < detailPitch;
}

/* Helper for the below. */
static void testPointVisible(const Vec3<float>& point, const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible)
{
    /* Project the point.  This perspective projection
     * yields a viewport with the x co-ordinates
     * (-ar, ar) and the y co-ordinates (-1.0, 1.0).
     */
    Vec4<float> projectedPoint = camera.getProjection() * afk_vec4<float>(
        point.v[0], point.v[1], point.v[2], 1.0f);
    bool visible = (
        (projectedPoint.v[0] / projectedPoint.v[2]) >= -camera.ar &&
        (projectedPoint.v[0] / projectedPoint.v[2]) <= camera.ar &&
        (projectedPoint.v[1] / projectedPoint.v[2]) >= -1.0f &&
        (projectedPoint.v[1] / projectedPoint.v[2]) <= 1.0f);

    io_someVisible |= visible;
    io_allVisible &= visible;
}

void AFK_WorldCell::testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const
{
    /* Check whether this cell is actually visible, by
     * testing all 8 vertices and its midpoint.
     * TODO Is that enough?
     */
    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3],
        realCoord.v[1] + realCoord.v[3],
        realCoord.v[2] + realCoord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(afk_vec3<float>(
        realCoord.v[0] + realCoord.v[3] / 2.0f,
        realCoord.v[1] + realCoord.v[3] / 2.0f,
        realCoord.v[2] + realCoord.v[3] / 2.0f),
        camera, io_someVisible, io_allVisible);
}

void AFK_WorldCell::makeTerrain(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    float minCellSize,
    const Vec3<float> *forcedTint)
{
    if (cell.coord.v[1] == 0 && !hasTerrain)
    {
        /* TODO RNG in thread local storage so I don't have to re-make
         * ones on the stack?  (inefficient?)
         */
        AFK_Boost_Taus88_RNG rng;

        /* Reserve some terrain space. */
        terrain.reserve(5);

        /* Make the terrain cell for this actual cell. */
        rng.seed(cell.rngSeed());
        boost::shared_ptr<AFK_TerrainCell> tcell(new AFK_TerrainCell());
        tcell->make(cell.toWorldSpace(minCellSize), pointSubdivisionFactor, subdivisionFactor, minCellSize, rng, forcedTint);
        terrain.push_back(tcell);

        /* Now, make the terrain for the four 1/2-cells that
         * overlap this cell
         */
        AFK_Cell halfCells[4];
        cell.enumerateHalfCells(&halfCells[0], 4);
        for (unsigned int i = 0; i < 4; ++i)
        {
            rng.seed(halfCells[i].rngSeed());
            boost::shared_ptr<AFK_TerrainCell> hcell(new AFK_TerrainCell());
            hcell->make(halfCells[i].toWorldSpace(minCellSize), pointSubdivisionFactor, subdivisionFactor, minCellSize, rng, forcedTint);
            terrain.push_back(hcell);
        }

        hasTerrain = true;
    }
}

void AFK_WorldCell::buildTerrainList(
    AFK_TerrainList& list,
    std::vector<AFK_Cell>& missing,
    float maxDistance,
    const AFK_WorldCache *cache) const
{
    /* Add the local terrain cells to the list. */
    for (std::vector<boost::shared_ptr<AFK_TerrainCell> >::const_iterator it = terrain.begin();
        it != terrain.end(); ++it)
    {
        list.add(*it);
    }

    /* If this isn't the top level cell... */
    if (/* !topLevel */ cell.coord.v[3] < maxDistance)
    {
        /* Find the next cell up.
         * If it's not here I'll throw an exception; the caller
         * needs to be able to cope (probably by gapping this
         * cell for this frame).
         * TODO: Should I forcibly enqueue it, and indeed can I
         * enqueue a dependent work item that recalculates this
         * terrain afterwards, to avoid lots of gaps?
         */
        bool foundNextCell = false;
        for (AFK_Cell nextCell = cell.parent();
            !foundNextCell && (float)nextCell.coord.v[3] < (maxDistance * 2.0f);
            nextCell = nextCell.parent())
        {
            try
            {
                AFK_WorldCell& parentWorldCell = cache->at(nextCell);
                parentWorldCell.buildTerrainList(list, missing, maxDistance, cache);
                foundNextCell = true;
            }
            catch (AFK_PolymerOutOfRange)
            {
                missing.push_back(nextCell);
            }
        }
    }
}

AFK_Cell AFK_WorldCell::terrainRoot(void) const
{
    if (cell.coord.v[1] == 0)
        return cell;
    else
        return afk_cell(afk_vec4<long long>(
            cell.coord.v[0], 0ll, cell.coord.v[2], cell.coord.v[3]));
}

bool AFK_WorldCell::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    bool canEvict = ((afk_core.renderingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell)
{
    /* TODO Something more descriptive might be nice */
    return os << "World cell (last seen " << worldCell.lastSeen << ", claimed by " << worldCell.claimingThreadId << ")";
}
