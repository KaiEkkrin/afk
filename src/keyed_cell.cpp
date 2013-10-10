/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/functional/hash.hpp>

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

size_t hash_value(const AFK_KeyedCell& cell)
{
    size_t hash = (size_t)cell.key;
    boost::hash_combine(hash, hash_value(cell.c));
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

