/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ENTITY_H_
#define _AFK_ENTITY_H_

#include "afk.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "cell.hpp"
#include "data/claimable.hpp"
#include "data/fair.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "rng/rng.hpp"
#include "shape.hpp"

/* An Entity is a moveable object that exists within the
 * world cells.
 * An Entity is Claimable: the worker thread should take
 * out an exclusive claim on it to ensure each one is
 * only processed once per frame.  It isn't cached, though:
 * instead they are stored within the world cells.
 * An Entity does not have its own geometry.  Instead, it
 * refers to a Shape (which does).  The idea is that I'll
 * make many Entities with the same Shapes, and use
 * OpenGL geometry instancing to draw them all at once.
 * If I don't do that, I won't be able to produce the swarms
 * of Entities that I really want to be able to produce. :)
 */
class AFK_Entity: public AFK_Claimable
{
protected:
    /* This object's shape.  We don't own this pointer -- it's
     * from the world shape list.
     */
    AFK_Shape *shape;

    /* Describes where this entity is located. */
    AFK_Object obj;

    /* TODO: I need to sort out entity movement.  This should
     * be done in OpenCL with a large queue of entities to
     * move, all of which get computed at once, a little like
     * the other compute queues.
     * And not here.
     * This probably means that the Entity needs to have a
     * reference into a large OpenCL blob that stores
     * displacement information, rather than that Object
     * instance above. :)
     */

public:
    virtual ~AFK_Entity();

    /* Positions the entity within a cell.
     * TODO Starting orientations and all those good things.  Really,
     * could be anything, because that's mostly before physics
     * should take over ...
     */
    void position(const AFK_Cell& cell, float minCellSize);

    /* Pushes the display units for this entity into the
     * display fair.
     * (Each shape will create a number of units, depending both
     * on the shape content, and the LoD it's displayed at...)
     */
    void enqueueDisplayUnits(
        const AFK_Cell& cell,
        float minCellSize,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair);

    /* AFK_Claimable implementation. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;
};

#endif /* _AFK_ENTITY_H_ */

