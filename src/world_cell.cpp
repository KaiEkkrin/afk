/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "exception.hpp"
#include "world_cell.hpp"


#define MAX_ENTITIES 4

/* This number defines how many cells (out of a total of
 * NO_ENTITIES_BIAS+1 cells, roughly) end up without
 * entities generated by doStartingEntities().
 */
#define NO_ENTITIES_BIAS 4

AFK_WorldCell::AFK_WorldCell():
    AFK_Claimable(),
    moveQueue(MAX_ENTITIES), /* I don't expect very many */
    startingEntitiesDone(false)
{
}

AFK_WorldCell::~AFK_WorldCell()
{
    for (AFK_ENTITY_LIST::iterator eIt = entities.begin();
        eIt != entities.end(); ++eIt)
    {
        delete *eIt;
    }

    /* I also own the contents of the move list.  All entries
     * have already been removed from their old owner cells.
     */
    AFK_Entity *e;
    while (moveQueue.pop(e)) delete e;
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

void AFK_WorldCell::doStartingEntities(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    AFK_RNG& rng)
{
    if (!startingEntitiesDone)
    {
        /* Come up with an entity count.  For now it will
         * usually be none, and have decreasing probabilities
         * from 1 up to max.
         */
#if 0
        float invMaxEntities = 1.0f / (float)MAX_ENTITIES;       
        float entityCountRandomRange = (float)NO_ENTITIES_BIAS - invMaxEntities;
        float invEntityCount = rng.frand() * entityCountRandomRange + invMaxEntities;
        unsigned int entityCount = (unsigned int)(1.0f / invEntityCount);
#endif

        for (unsigned int i = 0; i < /* entityCount */ 1; ++i)
        {
            AFK_Entity *e = new AFK_Entity();
            e->make(pointSubdivisionFactor, subdivisionFactor, rng);
            entities.push_back(e);
        }

        startingEntitiesDone = true;
    }
}

AFK_ENTITY_LIST::iterator AFK_WorldCell::entitiesBegin(void)
{
    return entities.begin();
}

AFK_ENTITY_LIST::iterator AFK_WorldCell::entitiesEnd(void)
{
    return entities.end();
}

AFK_ENTITY_LIST::iterator AFK_WorldCell::eraseEntity(AFK_ENTITY_LIST::iterator eIt)
{
    return entities.erase(eIt);
}

void AFK_WorldCell::moveEntity(AFK_Entity *entity)
{
    moveQueue.push(entity);
}

void AFK_WorldCell::popMoveQueue(void)
{
    AFK_Entity *e;
    while (moveQueue.pop(e))
        entities.push_back(e);
}

AFK_Frame AFK_WorldCell::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_WorldCell::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    /* TODO Should I check contained entities too?  For now,
     * I don't think so.  They'll always get hit along with the
     * cell.
     * However, when I get persistent entities, the cell homing
     * them should always return false (or at least, be much
     * more likely to return false ......)
     */
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 60);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell)
{
    /* TODO Something more descriptive might be nice */
    return os << "World cell (last seen " << worldCell.lastSeen << ")";
}

