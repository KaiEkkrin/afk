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

#ifndef _AFK_JIGSAW_CUBOID_H_
#define _AFK_JIGSAW_CUBOID_H_

#include "afk.hpp"

#include <sstream>

#include "def.hpp"

/* A JigsawCuboid is a simple spatial subdivision of a jigsaw, in
 * piece units.
 */
class AFK_JigsawCuboid
{
public:
    Vec3<int> location;
    Vec3<int> size;

    AFK_JigsawCuboid();
    AFK_JigsawCuboid(const Vec3<int>& _location, const Vec3<int>& _size);

    operator bool() const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& cuboid);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& cuboid);

#endif /* _AFK_JIGSAW_CUBOID_H_ */

