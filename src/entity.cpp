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

AFK_Entity::AFK_Entity(): shapeKey(0) {}

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

