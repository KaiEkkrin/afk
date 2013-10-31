/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_ENTITY_H_
#define _AFK_ENTITY_H_

#include "afk.hpp"

#include <boost/date_time/posix_time/posix_time.hpp>

#include "cell.hpp"
#include "data/claimable.hpp"
#include "data/fair.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw_collection.hpp"
#include "object.hpp"
#include "rng/rng.hpp"
#include "work.hpp"

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
class AFK_Entity
{
public:
    AFK_Claimable claimable;

protected:
    /* The shape key is used to vary the RNG output to generate
     * the shape, and keys the shape cell and vapour cell
     * caches.
     */
    const unsigned int shapeKey;

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
    AFK_Entity(unsigned int _shapeKey);

    /* I need this for the std::list */
    AFK_Entity(const AFK_Entity& _entity);

    virtual ~AFK_Entity();

    unsigned int getShapeKey(void) const;

    /* Positions the entity.
     */
    void position(
        const Vec3<float>& scale,
        const Vec3<float>& displacement,
        const Vec3<float>& rotation /* pitch, yaw, roll */
        );

    Mat4<float> getTransformation(void) const { return obj.getTransformation(); }
};

#endif /* _AFK_ENTITY_H_ */

