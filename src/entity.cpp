/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "displayed_entity.hpp"
#include "entity.hpp"
#include "exception.hpp"

AFK_Entity::AFK_Entity():
    geometry(NULL)
{
}

AFK_Entity::~AFK_Entity()
{
    if (geometry) delete geometry;
}

void AFK_Entity::make(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    AFK_RNG& rng)
{
    /* TODO: Fill out `geometry' with, you know, actual
     * geometry for the entity.  I'm going to have to do
     * some thinking about this.
     */

    /* TODO: Decide what I want to do here, w.r.t.
     * randomly spawned entities.  For now, I'm going to
     * give everything a default movement ...
     */
    velocity = afk_vec3<float>(0.0f, 1.0f / 1000.0f, 0.0f);
    angularVelocity = afk_vec3<float>(0.0f, 0.0f, 1.0f / 1000.0f);
}

bool AFK_Entity::animate(
    const boost::posix_time::ptime& now,
    const AFK_Cell& cell,
    float minCellSize,
    AFK_Cell& o_newCell)
{
    bool moved = false;

    if (!lastMoved.is_not_a_date_time())
    {
        boost::posix_time::time_duration sinceLastMoved = now - lastMoved;
        unsigned int slmMicros = sinceLastMoved.total_microseconds();
        float fSlmMillis = (float)slmMicros / 1000.0f;

        obj.drive(velocity * fSlmMillis, angularVelocity * fSlmMillis);
    
        /* Now that I've driven the object, check its real world
         * position to see if it's still within cell bounds.
         * TODO: For complex entities that are split across cells,
         * I'm going to want to do this for all the entity cells;
         * but I don't have that concept right now.
         */
        Vec4<float> newLocationH = obj.getTransformation() *
            afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
        Vec3<float> newLocation = afk_vec3<float>(
            newLocationH.v[0] * newLocationH.v[3],
            newLocationH.v[1] * newLocationH.v[3],
            newLocationH.v[2] * newLocationH.v[3]);
    
        o_newCell = afk_cellContaining(newLocation, cell.coord.v[3], minCellSize);
        moved = (o_newCell != cell);
    }

    lastMoved = now;
    return moved;
}

AFK_DisplayedEntity *AFK_Entity::makeDisplayedEntity(void)
{
    AFK_DisplayedEntity *de = NULL;

    if (geometry)
    {
        de = new AFK_DisplayedEntity(&geometry, &obj);
    }

    return de;
}

AFK_Frame AFK_Entity::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_Entity::canBeEvicted(void) const
{
    /* We shouldn't actually be calling this.  I'm using
     * Claimable for the frame-tracking claim utility,
     * not as an evictable thing.
     */
    throw new AFK_Exception("Wanted to evict an Entity");
}

