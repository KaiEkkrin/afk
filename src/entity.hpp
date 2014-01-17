/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include <sstream>

#include "data/frame.hpp"
#include "def.hpp"
#include "object.hpp"

/* An Entity is a moveable object that exists within the
 * world cells.
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
    /* The shape key is used to vary the RNG output to generate
     * the shape, and keys the shape cell and vapour cell
     * caches.
     */
    unsigned int shapeKey;

protected:
    /* Describes where this entity is located. */
    AFK_Object obj;

    /* The last frame this entity was processed.
     * This doesn't need to be an atomic or anything -- the caller
     * will already have a claim on the containing WorldCell.
     */
    int64_t lastFrameId;

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
    AFK_Entity();

    /* Positions the entity.
     */
    void position(
        const Vec3<float>& scale,
        const Vec3<float>& displacement,
        const Vec3<float>& rotation /* pitch, yaw, roll */
        );

    /* Checks whether it's been processed this frame -- if not,
     * returns true and bumps the last frame processed.
     */
    bool notProcessedYet(const AFK_Frame& currentFrame);

    Mat4<float> getTransformation(void) const { return obj.getTransformation(); }

    friend std::ostream& operator<<(std::ostream& os, const AFK_Entity& _entity);
};

std::ostream& operator<<(std::ostream& os, const AFK_Entity& _entity);

#endif /* _AFK_ENTITY_H_ */

