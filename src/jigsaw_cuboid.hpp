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

#ifndef _AFK_JIGSAW_CUBOID_H_
#define _AFK_JIGSAW_CUBOID_H_

#include "afk.hpp"

#include <sstream>

#include <boost/atomic.hpp>

/* This class encapsulates a cuboid within the jigsaw that
 * contains all the updates from a frame (we write to it) or all
 * the draws (we use it to sort out rectangular reads and writes
 * of the texture itself).
 */
class AFK_JigsawCuboid
{
public:
    /* These co-ordinates are in piece units.
     * During usage, a cuboid can't grow in rows or slices, but it
     * can grow in columns.
     */
    int r, c, s, rows, slices;
    boost::atomic_int columns;

    AFK_JigsawCuboid(int _r, int _c, int _s, int _rows, int _slices);

    AFK_JigsawCuboid(const AFK_JigsawCuboid& other);
    AFK_JigsawCuboid& operator=(const AFK_JigsawCuboid& other);

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& sr);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& sr);

#endif /* _AFK_JIGSAW_CUBOID_H_ */

