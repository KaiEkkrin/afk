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

Vec4<float> AFK_WorldCell::getRealCoord(void) const
{
    return visibleCell.getRealCoord();
}

void AFK_WorldCell::bind(const AFK_Cell& _cell, float _worldScale)
{
    cell = _cell;
    /* TODO If this actually *changes* something, I need to
     * clear the terrain so that it can be re-made.
     */
    visibleCell.bindToCell(cell, _worldScale);
}

bool AFK_WorldCell::testDetailPitch(
    float detailPitch,
    const AFK_Camera& camera,
    const Vec3<float>& viewerLocation) const
{
    return visibleCell.testDetailPitch(detailPitch, camera, viewerLocation);
}

void AFK_WorldCell::testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const
{
    return visibleCell.testVisibility(camera, io_someVisible, io_allVisible);
}

unsigned int AFK_WorldCell::getStartingEntitiesWanted(
    AFK_RNG& rng,
    unsigned int maxEntitiesPerCell,
    unsigned int entitySparseness) const
{
    unsigned int entityCount = 0;
    if (entities.size() == 0)
    {
        /* TODO Debug of specific entity */
        if (cell.coord.v[0] == 0 && cell.coord.v[1] == 0 && cell.coord.v[2] == 0 && cell.coord.v[3] == 16)
        {
            entityCount = 1;
        }
        else
        {
#if 1
            Vec4<float> realCoord = getRealCoord();
            float sparseMult = log(realCoord.v[3]);
#else
            float sparseMult = 1.0f;
#endif

            for (unsigned int entitySlot = 0; entitySlot < maxEntitiesPerCell; ++entitySlot)
            {
                if (rng.frand() < (sparseMult / ((float)entitySparseness)))
                    ++entityCount;
            }
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
    unsigned int shapeKey,
    const AFK_ShapeSizes& sSizes,
    AFK_RNG& rng)
{
    AFK_Entity *e = new AFK_Entity(shapeKey);

    Vec4<float> realCoord = getRealCoord();

    /* Fit the entity inside the cell, but don't make it so
     * small as to be a better fit for sub-cells
     */
    //float maxEntitySize = realCoord.v[3] / (/* (float)sSizes.entitySubdivisionFactor * */ (float)sSizes.skeletonFlagGridDim);
    float maxEntitySize = realCoord.v[3] / (float)sSizes.skeletonFlagGridDim;
    float minEntitySize = maxEntitySize / sSizes.subdivisionFactor;

    float entitySize = rng.frand() * (maxEntitySize - minEntitySize) + minEntitySize;

    float minEntityLocation = entitySize;
    float maxEntityLocation = realCoord.v[3] - 2.0f * entitySize;

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

