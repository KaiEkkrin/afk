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

#include <cassert>

#include "jigsaw_cuboid.hpp"

/* AFK_JigsawCuboid implementation */

AFK_JigsawCuboid::AFK_JigsawCuboid():
    location(afk_vec3<int>(-1, -1, -1)), size(afk_vec3<int>(-1, -1, -1))
{
}

AFK_JigsawCuboid::AFK_JigsawCuboid(const Vec3<int>& _location, const Vec3<int>& _size):
    location(_location), size(_size)
{
}

AFK_JigsawCuboid::operator bool() const
{
    bool isSet = (location.v[0] >= 0);

    /* Sanity checking -- the negatives should only
     * ever come together
     */
    if (!isSet)
        assert(location.v[1] < 0 && location.v[2] < 0 &&
            size.v[0] < 0 && size.v[1] < 0 && size.v[2] < 0);
    else
        assert(location.v[1] >= 0 && location.v[2] >= 0 &&
            size.v[0] >= 0 && size.v[1] >= 0 && size.v[2] >= 0);

    return isSet;
}

