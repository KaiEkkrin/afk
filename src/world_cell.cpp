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

void AFK_WorldCell::computeTerrainRec(Vec3<float> *positions, Vec3<float> *colours, size_t length, AFK_WorldCache *cache) const
{
    /* Co-ordinates arrive in the context of this cell's
     * first terrain element.
     * Compute all of them, transforming between them as
     * required
     */
    for (unsigned int i = 0; i < TERRAIN_CELLS_PER_CELL; ++i)
    {
        if (i > 0)
        {
            /* Need to do the next transformation. */
            terrain[i-1].transformCellToCell(positions, length, terrain[i]);
        }

        terrain[i].compute(positions, colours, length);
    }

    if (topLevel)
    {
        /* Transform back into world space. */
        Vec4<float> cellCoord = terrain[TERRAIN_CELLS_PER_CELL-1].getCellCoord();
        Vec3<float> cellXYZ = afk_vec3<float>(cellCoord.v[0], cellCoord.v[1], cellCoord.v[2]);

        for (size_t i = 0; i < length; ++i)
            positions[i] = (positions[i] * cellCoord.v[3]) + cellXYZ;
    }
    else
    {
        /* Find the next cell up.
         * If it isn't there something has gone rather wrong,
         * because I should have touched it earlier
         */
        AFK_WorldCell& parentWorldCell = cache->at(cell.parent());
        if (!parentWorldCell.hasTerrain) throw AFK_WorldCellNotPresentException();

        /* Transform into the co-ordinates of this cell's
         * first terrain element
         */
        terrain[TERRAIN_CELLS_PER_CELL-1].transformCellToCell(
            positions, length, parentWorldCell.terrain[0]);

        /* ...and compute its terrain too */
        parentWorldCell.computeTerrainRec(positions, colours, length, cache);
    }
}

AFK_WorldCell::AFK_WorldCell():
    claimingThreadId(UNCLAIMED), hasTerrain(false), displayed(NULL)
{
}

AFK_WorldCell::~AFK_WorldCell()
{
    if (displayed) delete displayed;
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
            lastSeen = afk_core.computingFrame;
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

void AFK_WorldCell::bind(const AFK_Cell& _cell, bool _topLevel, float _worldScale)
{
    cell = _cell;
    topLevel = _topLevel;
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

        /* Make the terrain cell for this actual cell. */
        rng.seed(cell.rngSeed());
        terrain[0].make(cell.toWorldSpace(minCellSize), pointSubdivisionFactor, subdivisionFactor, minCellSize, rng, forcedTint);

        /* Now, make the terrain for the four 1/2-cells that
         * overlap this cell
         */
        AFK_Cell halfCells[4];
        cell.enumerateHalfCells(&halfCells[0], 4);
        for (unsigned int i = 0; i < 4; ++i)
        {
            rng.seed(halfCells[i].rngSeed());
            terrain[i+1].make(halfCells[i].toWorldSpace(minCellSize), pointSubdivisionFactor, subdivisionFactor, minCellSize, rng, forcedTint);
        }

        hasTerrain = true;
    }
}

void AFK_WorldCell::computeTerrain(Vec3<float> *positions, Vec3<float> *colours, size_t length, AFK_WorldCache *cache) const
{
    if (terrain)
    {
        /* Make positions in the space of the first terrain cell */
        Vec4<float> firstTerrainCoord = terrain[0].getCellCoord();
        Vec3<float> firstTerrainXYZ = afk_vec3<float>(firstTerrainCoord.v[0], firstTerrainCoord.v[1], firstTerrainCoord.v[2]);

        for (size_t i = 0; i < length; ++i)
            positions[i] = (positions[i] - firstTerrainXYZ) / firstTerrainCoord.v[3];

        computeTerrainRec(positions, colours, length, cache);

        /* Put the colours into shape.
         * TODO This really wants tweaking; the normalise produces
         * pastels.  Maybe something logarithmic?
         */
        for (size_t i = 0; i < length; ++i)
        {
            /* colours[i] = (colours[i].normalise() + 1.0f) / 2.0f; */
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
