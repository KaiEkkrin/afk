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
    /* TODO A bit like landscape_tile, am I going to want an
     * entity descriptor here, to be rendered (hopefully by
     * the GPU ...) into entity geometry?
     */

    /* This object's shape.  We don't own this pointer -- it's
     * from the world shape list.
     */
    AFK_Shape *shape;

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
    virtual ~AFK_Entity();

    /* Makes the entity.  Basically, it places the given
     * shape within the cell.
     */
    void make(
        AFK_Shape *_shape,
        const AFK_Cell& cell,
        float minCellSize,
        unsigned int pointSubdivisionFactor,
        unsigned int subdivisionFactor,
        AFK_RNG& rng);

    /* Animates the entity.  If this causes the entity to
     * move out of its cell, fills out the co-ordinates of the
     * new cell it should go to and returns true; otherwise,
     * returns false.
     * TODO: In future after I've got the entity drawing
     * stuff nailed nicely, I'd like to be able to place
     * all the Entities into an array and have OpenCL
     * animate them all at once.  That would be much nicer...
     */
    bool animate(
        const boost::posix_time::ptime& now,
        const AFK_Cell& cell,
        float minCellSize,
        AFK_Cell& o_newCell);

    /* Enqueues this entity for drawing. */
    void enqueueForDrawing(unsigned int threadId);

    /* AFK_Claimable implementation. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;
};

#endif /* _AFK_ENTITY_H_ */

