/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "exception.hpp"
#include "world_cell.hpp"


AFK_WorldCell::AFK_WorldCell():
    AFK_Claimable(),
    moveQueue(32) /* I don't expect very many */
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
    return camera.getDetailPitchAsSeen(realCoord.v[3], distanceToViewer) < detailPitch;
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
    bool visible = camera.projectedPointIsVisible(projectedPoint);

    io_someVisible |= visible;
    io_allVisible &= visible;
}

void AFK_WorldCell::testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const
{
    /* Check whether this cell is actually visible, by
     * testing all 8 vertices and its midpoint.
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

unsigned int AFK_WorldCell::getStartingEntitiesWanted(
    AFK_RNG& rng,
    unsigned int maxEntitiesPerCell,
    unsigned int entitySparseness) const
{
    unsigned int entityCount = 0;
    if (entities.size() == 0)
    {
        for (unsigned int entitySlot = 0; entitySlot < maxEntitiesPerCell; ++entitySlot)
        {
            if (rng.frand() < (1.0f / ((float)entitySparseness)))
                ++entityCount;
        }
    }

    return entityCount;
}

unsigned int AFK_WorldCell::getStartingEntityShapeKey(AFK_RNG& rng)
{
    /* TODO: This ought to be tweakable -- number of distinct
     * starting shapes to juggle -- but it's very dependent on
     * exactly what kind of world I'm trying to make!
     */
    return (rng.uirand() & 0xf);
}

void AFK_WorldCell::addStartingEntity(
    AFK_Shape *shape,
    const AFK_ShapeSizes& sSizes,
    AFK_RNG& rng)
{
    AFK_Entity *e = new AFK_Entity(shape);

    /* Fit the entity inside the cell, but don't make it so
     * small as to be a better fit for sub-cells
     */
    float maxEntitySize = realCoord.v[3] / (/* (float)sSizes.entitySubdivisionFactor * */ (float)sSizes.skeletonFlagGridDim);
    float minEntitySize = maxEntitySize / sSizes.subdivisionFactor;

    float entitySize = rng.frand() * (maxEntitySize - minEntitySize) + minEntitySize;

    float minEntityLocation = 0.0f;
    float maxEntityLocation = realCoord.v[3];

    Vec3<float> entityDisplacement;
    for (unsigned int j = 0; j < 3; ++j)
    {
        entityDisplacement.v[j] = realCoord.v[j] + rng.frand() * (maxEntityLocation - minEntityLocation) + minEntityLocation;
    }

    Vec3<float> entityRotation;
    switch (rng.uirand() % 5)
    {
    case 0:
        entityRotation = afk_vec3<float>(0.0f, 0.0f, 0.0f);
        break;

    case 1:
        entityRotation = afk_vec3<float>(rng.frand() * 2.0f * M_PI, 0.0f, 0.0f);
        break;

    case 2:
        entityRotation = afk_vec3<float>(0.0f, rng.frand() * 2.0f * M_PI, 0.0f);
        break;

    case 3:
        entityRotation = afk_vec3<float>(0.0f, 0.0f, rng.frand() * 2.0f * M_PI);
        break;

    default:
        entityRotation = afk_vec3<float>(
            rng.frand() * 2.0f * M_PI,
            rng.frand() * 2.0f * M_PI,
            rng.frand() * 2.0f * M_PI);
        break;
    }

    e->position(
        afk_vec3<float>(entitySize, entitySize, entitySize),
        entityDisplacement,
        entityRotation);

    entities.push_back(e);
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
    return os << "World cell (last seen " << worldCell.lastSeen << ")";
}

