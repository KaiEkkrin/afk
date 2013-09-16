/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/functional/hash.hpp>

#include "core.hpp"
#include "keyed_cell.hpp"

/* AFK_KeyedCell implementation */

bool AFK_KeyedCell::operator==(const AFK_KeyedCell& _cell) const
{
    return AFK_Cell::operator==(_cell) && key == _cell.key;
}

bool AFK_KeyedCell::operator!=(const AFK_KeyedCell& _cell) const
{
    return AFK_Cell::operator!=(_cell) || key != _cell.key;
}

AFK_RNG_Value AFK_KeyedCell::rngSeed() const
{
    size_t hash = hash_value(*this);
    return AFK_RNG_Value(hash) ^ afk_core.config->masterSeed;
}

AFK_RNG_Value AFK_KeyedCell::rngSeed(size_t combinant) const
{
    size_t hash = combinant;
    boost::hash_combine(hash, hash_value(*this));
    return AFK_RNG_Value(hash) ^ afk_core.config->masterSeed;
}

unsigned int AFK_KeyedCell::subdivide(AFK_KeyedCell *subCells, const size_t subCellsSize, unsigned int subdivisionFactor) const
{
    /* TODO Do this without thrashing the heap :/ */
    AFK_Cell *csc = new AFK_Cell[subCellsSize];
    unsigned int count = AFK_Cell::subdivide(csc, subCellsSize, subdivisionFactor);
    unsigned int i;
    for (i = 0; i < count && i < subCellsSize; ++i)
    {
        subCells[i] = afk_keyedCell(csc[i], key);
    }

    return i;
}

AFK_KeyedCell AFK_KeyedCell::parent(unsigned int subdivisionFactor) const
{
    return afk_keyedCell(
        AFK_Cell::parent(subdivisionFactor),
        key);
}

bool AFK_KeyedCell::isParent(const AFK_KeyedCell& parent) const
{
    return parent.key == key && AFK_Cell::isParent(parent);
}

AFK_KeyedCell afk_keyedCell(const AFK_KeyedCell& other)
{
    AFK_KeyedCell cell;
    cell.coord = other.coord;
    cell.key = other.key;
    return cell;
}

AFK_KeyedCell afk_keyedCell(const AFK_Cell& _baseCell, long long _key)
{
    AFK_KeyedCell cell;
    cell.coord = _baseCell.coord;
    cell.key = _key;
    return cell;
}

AFK_KeyedCell afk_keyedCell(const Vec4<long long>& _coord, long long _key)
{
    AFK_KeyedCell cell;
    cell.coord = _coord;
    cell.key = _key;
    return cell;
}

size_t hash_value(const AFK_KeyedCell& cell)
{
    size_t hash = cell.key;
    boost::hash_combine(hash, (const AFK_Cell)cell);
    return hash;
}


std::ostream& operator<<(std::ostream& os, const AFK_KeyedCell& cell)
{
    return os << "Cell(" << std::dec <<
        cell.coord.v[0] << ", " <<
        cell.coord.v[1] << ", " <<
        cell.coord.v[2] << ", scale " <<
        cell.coord.v[3] << ", key " <<
        cell.key << ")";
}

