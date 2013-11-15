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

#include "jigsaw_cuboid.hpp"


/* AFK_JigsawCuboid implementation */

AFK_JigsawCuboid::AFK_JigsawCuboid(int _r, int _c, int _s, int _rows, int _slices):
    r(_r), c(_c), s(_s), rows(_rows), columns(0), slices(_slices)
{
}

AFK_JigsawCuboid::AFK_JigsawCuboid(const AFK_JigsawCuboid& other):
    r(other.r), c(other.c), s(other.s),
    rows(other.rows), columns(other.columns), slices(other.slices)
{
}

AFK_JigsawCuboid& AFK_JigsawCuboid::operator=(const AFK_JigsawCuboid& other)
{
    r = other.r;
    c = other.c;
    s = other.s;
    rows = other.rows;
    columns = other.columns;
    slices = other.slices;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& sr)
{
    os << "JigsawCuboid(";
    os << "r=" << std::dec << sr.r;
    os << ", c=" << sr.c;
    os << ", s=" << sr.s;
    os << ", rows=" << sr.rows;
    os << ", columns=" << sr.columns;
    os << ", slices=" << sr.slices << ")";
    return os;
}

