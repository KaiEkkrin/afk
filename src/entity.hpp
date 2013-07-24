/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ENTITY_H_
#define _AFK_ENTITY_H_

#include "afk.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "cell.hpp"
#include "data/claimable.hpp"
#include "def.hpp"
#include "display.hpp"
#include "object.hpp"
#include "rng/rng.hpp"

class AFK_DisplayedEntity;

/* This class contains an entity's geometry buffers. */
class AFK_EntityGeometry
{
public:
    AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>     vs;
    AFK_DisplayedBuffer<Vec3<unsigned int> >            is;

    AFK_EntityGeometry(size_t vCount, size_t iCount);
};

/* An Entity is a moveable object that exists within the
 * world cells.
 * An Entity is Claimable: the worker thread should take
 * out an exclusive claim on it to ensure each one is
 * only processed once per frame.  It isn't cached, though:
 * instead they are stored within the world cells.
 */
class AFK_Entity: public AFK_Claimable
{
protected:
    /* TODO A bit like landscape_tile, am I going to want an
     * entity descriptor here, to be rendered (hopefully by
     * the GPU ...) into entity geometry?
     */

    /* This object's geometry.  (Owned by the entity,
     * flushed along with it.)
     * TODO: As per this design, every entity is unique. That's
     * great, but not very scalable.  How about having lots of
     * entities and a smaller number of different geometries,
     * so that the identical ones can be replicated by the
     * GPU using instancing?
     */
    AFK_EntityGeometry *geometry;

    /* Describes where this entity is located. */
    AFK_Object obj;

    /* The next fields describe some simple motion for the
     * entity.  I'm not sure I'm going to want to keep this
     * here.  In future, I'll want plenty of entities that do
     * move like this (snow, debris, that kind of thing), but
     * I'll also want an AI that controls movement -- or will
     * I want to hook that in separately?
     * Anyway, these are in units of movement/millisecond.
     */
    Vec3<float> velocity;
    Vec3<float> angularVelocity; /* pitch, yaw, roll */

    /* The time at which this object was last moved. */
    boost::posix_time::ptime lastMoved;

public:
    AFK_Entity();
    virtual ~AFK_Entity();

    /* Makes the entity's geometry. */
    void make(
        unsigned int pointSubdivisionFactor,
        unsigned int subdivisionFactor,
        AFK_RNG& rng);

    /* Animates the entity.  If this causes the entity to
     * move out of its cell, fills out the co-ordinates of the
     * new cell it should go to and returns true; otherwise,
     * returns false.
     */
    bool animate(const boost::posix_time::ptime& now, const AFK_Cell& cell, float minCellSize, AFK_Cell& o_newCell);

    /* Allocates and returns the DisplayedEntity object that
     * can actually render this entity.
     * I've done it like this because although there aren't
     * now, in future an entity will be rendered within a
     * context that changes every frame (applicable lights,
     * shadows, etc.)
     */
    AFK_DisplayedEntity *makeDisplayedEntity(void);

    /* AFK_Claimable implementation. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;
};

#endif /* _AFK_ENTITY_H_ */

