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

#ifndef _AFK_KEYED_CELL_H_
#define _AFK_KEYED_CELL_H_

#include "afk.hpp"

#include "cell.hpp"
#include "def.hpp"

/* A KeyedCell is like a cell but with an additional "key" value
 * that specifies which thing out of its category it refers to
 * (for shapes etc).
 */

class AFK_KeyedCell
{
public:
    AFK_Cell c;
    int64_t key;

    bool operator==(const AFK_KeyedCell& _cell) const;
    bool operator!=(const AFK_KeyedCell& _cell) const;

    AFK_RNG_Value rngSeed() const;

    AFK_KeyedCell parent(unsigned int subdivisionFactor) const;
    bool isParent(const AFK_KeyedCell& parent) const;

    Vec4<float> toWorldSpace(float worldScale) const { return c.toWorldSpace(worldScale); }
    Vec4<float> toHomogeneous(float worldScale) const { return c.toHomogeneous(worldScale); }
};

AFK_KeyedCell afk_keyedCell(const AFK_KeyedCell& other);
AFK_KeyedCell afk_keyedCell(const AFK_Cell& _baseCell, int64_t _key);
AFK_KeyedCell afk_keyedCell(const Vec4<int64_t>& _coord, int64_t _key);

/* A default "unassigned" keyed cell value for polymer keying. */
extern const AFK_KeyedCell afk_unassignedKeyedCell;

size_t hash_value(const AFK_KeyedCell& cell);

struct AFK_HashKeyedCell
{
    size_t operator()(const AFK_KeyedCell& cell) const { return hash_value(cell); }
};

std::ostream& operator<<(std::ostream& os, const AFK_KeyedCell& cell);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_KeyedCell>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<AFK_KeyedCell>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_KeyedCell>::value));

#endif /* _AFK_KEYED_CELL_H_ */

