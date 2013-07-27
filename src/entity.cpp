/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "entity.hpp"
#include "exception.hpp"


/* AFK_Entity implementation */

AFK_Entity::~AFK_Entity()
{
}

void AFK_Entity::make(
    AFK_Shape *_shape,
    const AFK_Cell& cell,
    float minCellSize,
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    AFK_RNG& rng)
{
    shape = _shape;

    /* For now, I'm going to treat entities like the
     * landscape and just try to make a tapestry of them
     * at the "correct" LoD (hah).  This means that I will:
     * - scale them between (cell scale / pointSubdivisionFactor)
     * and that divided by subdivisionFactor;
     * - move them to a random place within the cell.
     */
    Vec4<float> realCoord = cell.toWorldSpace(minCellSize);

    float maxScale = realCoord.v[3] / (float)pointSubdivisionFactor;
    float minScale = maxScale / (float)subdivisionFactor;
    float objScale = rng.frand() * (maxScale - minScale) + minScale;

    obj.scale(afk_vec3<float>(objScale, objScale, objScale));

    obj.displace(afk_vec3<float>(
        realCoord.v[0] + rng.frand() * realCoord.v[3],
        realCoord.v[1] + rng.frand() * realCoord.v[3],
        realCoord.v[2] + rng.frand() * realCoord.v[3]));

    /* TODO: Decide what I want to do here, w.r.t.
     * randomly spawned entities.  For now, I'm going to
     * give everything a default movement ...
     */
    unsigned int decider = rng.uirand();
    velocity = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    angularVelocity = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    angularVelocity.v[decider % 3] = rng.frand() / 1000.0f;
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
            newLocationH.v[0] / newLocationH.v[3],
            newLocationH.v[1] / newLocationH.v[3],
            newLocationH.v[2] / newLocationH.v[3]);
    
        o_newCell = afk_cellContaining(newLocation, cell.coord.v[3], minCellSize);
        moved = (o_newCell != cell);
    }

    lastMoved = now;
    return moved;
}

void AFK_Entity::enqueueForDrawing(unsigned int threadId)
{
    shape->updatePush(threadId, obj.getTransformation());
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

