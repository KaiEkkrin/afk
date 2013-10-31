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

#include "afk.hpp"

#include "core.hpp"
#include "entity.hpp"
#include "exception.hpp"


/* AFK_Entity implementation */

AFK_Entity::AFK_Entity(unsigned int _shapeKey):
    shapeKey(_shapeKey)
{
}

/* TODO: This is a nasty mess, technically very wrong.  however,
 * it's currently only being used on uninitialised entities before
 * we Claim them in order to generate them (therefore we _want_ a
 * default Claimable with a lastSeen of never).  In future, the task
 * of moving entities about will no doubt end up in the OpenCL with
 * jigsaws to contain them, which should get rid of this problem
 * entirely.
 */
AFK_Entity::AFK_Entity(const AFK_Entity& _entity):
    claimable(),
    shapeKey(_entity.shapeKey),
    obj(_entity.obj)
{
}

AFK_Entity::~AFK_Entity()
{
}

unsigned int AFK_Entity::getShapeKey(void) const
{
    return shapeKey;
}

void AFK_Entity::position(
    const Vec3<float>& scale,
    const Vec3<float>& displacement,
    const Vec3<float>& rotation)
{
    obj.resize(scale);
    obj.displace(displacement);
    if (rotation.v[0] != 0.0f) obj.adjustAttitude(AXIS_PITCH, rotation.v[0]);
    if (rotation.v[1] != 0.0f) obj.adjustAttitude(AXIS_YAW, rotation.v[1]);
    if (rotation.v[2] != 0.0f) obj.adjustAttitude(AXIS_ROLL, rotation.v[2]);
}

