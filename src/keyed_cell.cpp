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
#include "keyed_cell.hpp"

/* AFK_KeyedCell implementation */

bool AFK_KeyedCell::operator==(const AFK_KeyedCell& _cell) const
{
    return (c == _cell.c && key == _cell.key);
}

bool AFK_KeyedCell::operator!=(const AFK_KeyedCell& _cell) const
{
    return (c != _cell.c || key != _cell.key);
}

AFK_RNG_Value AFK_KeyedCell::rngSeed() const
{
    return c.rngSeed(key);
}

AFK_KeyedCell AFK_KeyedCell::parent(unsigned int subdivisionFactor) const
{
    return afk_keyedCell(
        c.parent(subdivisionFactor),
        key);
}

bool AFK_KeyedCell::isParent(const AFK_KeyedCell& parent) const
{
    return parent.key == key && c.isParent(parent.c);
}

AFK_KeyedCell afk_keyedCell(const AFK_KeyedCell& other)
{
    AFK_KeyedCell cell;
    cell.c = other.c;
    cell.key = other.key;
    return cell;
}

AFK_KeyedCell afk_keyedCell(const AFK_Cell& _baseCell, int64_t _key)
{
    AFK_KeyedCell cell;
    cell.c = _baseCell;
    cell.key = _key;
    return cell;
}

AFK_KeyedCell afk_keyedCell(const Vec4<int64_t>& _coord, int64_t _key)
{
    AFK_KeyedCell cell;
    cell.c = afk_cell(_coord);
    cell.key = _key;
    return cell;
}

const AFK_KeyedCell afk_unassignedKeyedCell = afk_keyedCell(afk_vec4<int64_t>(0, 0, 0, -1), -1);

size_t hash_value(const AFK_KeyedCell& cell)
{
    size_t hash = (size_t)cell.key;
    hash = afk_hash_swizzle(hash, hash_value(cell.c));
    return hash;
}


std::ostream& operator<<(std::ostream& os, const AFK_KeyedCell& cell)
{
    return os << "Cell(" << std::dec <<
        cell.c.coord.v[0] << ", " <<
        cell.c.coord.v[1] << ", " <<
        cell.c.coord.v[2] << ", scale " <<
        cell.c.coord.v[3] << ", key " <<
        cell.key << ")";
}

