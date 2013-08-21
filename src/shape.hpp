/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_H_
#define _AFK_SHAPE_H_

#include "afk.hpp"

#include <vector>

#include "data/fair.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "jigsaw.hpp"

/* A Shape describes a single shrinkform shape, which
 * might be instanced many times by means of Entities.
 *
 * TODO: Should a shape be cached, and Claimable, as well?
 * I suspect it should.  But to test just a single Shape,
 * I don't need that.
 */
class AFK_Shape
{
protected:
    /* TODO: In order to generate a Shape's unique geometry,
     * I'm going to need a shrinkform descriptor here,
     * followed by a list of jigsaw pieces in the same order
     * (or maybe part of the descriptor),
     * a reference to the jigsaw collection itself,
     * and also the timestamps for all those jigsaw pieces.
     * But first, I want to test with just a flat cube to make
     * sure the GL pipeline is OK.
     */

public:
    /* Enqueues the display units for an entity of this shape. */
    void enqueueDisplayUnits(
        const AFK_Object& object,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair);
};

#endif /* _AFK_SHAPE_H_ */

