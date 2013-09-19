/* AFK (c) Alex Holloway 2013 */

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
    long long key;

    bool operator==(const AFK_KeyedCell& _cell) const;
    bool operator!=(const AFK_KeyedCell& _cell) const;

    AFK_RNG_Value rngSeed() const;

    AFK_KeyedCell parent(unsigned int subdivisionFactor) const;
    bool isParent(const AFK_KeyedCell& parent) const;

    Vec4<float> toWorldSpace(float worldScale) const { return c.toWorldSpace(worldScale); }
};

AFK_KeyedCell afk_keyedCell(const AFK_KeyedCell& other);
AFK_KeyedCell afk_keyedCell(const AFK_Cell& _baseCell, long long _key);
AFK_KeyedCell afk_keyedCell(const Vec4<long long>& _coord, long long _key);

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

