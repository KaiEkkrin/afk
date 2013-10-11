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

#ifndef _AFK_DATA_MAP_CACHE_H_
#define _AFK_DATA_MAP_CACHE_H_

#include <boost/thread/mutex.hpp>
#include <boost/unordered_map.hpp>

#include "cache.hpp"

/* This defines the AFK cache as a simple boost::unordered_map
 * guarded by a mutex.
 *
 * It's deprecated by the way.  I think I should probably just delete
 * it; I'm positive the Polymer is better for my purposes.
 */

template<class Key, class Value>
class AFK_MapCache: public AFK_Cache<Key, Value>
{
protected:
    boost::unordered_map<Key, Value> map;
    boost::mutex mut;

public:
    AFK_MapCache() {}
    virtual ~AFK_MapCache() {}

    virtual size_t size() const
    {
        return map.size();
    }

    virtual Value& operator[](const Key& key)
    {
        boost::unique_lock<boost::mutex> lock(mut);
        return map[key];
    }

#if 0
    virtual bool findDuplicate(const Key& key, const Value& value, Value& o_duplicateValue)
    {
        /* mapCache is uniquely keyed. */
        return false;
    }
#endif

    virtual void printEverything(std::ostream& os) const
    {
        for (typename boost::unordered_map<Key, Value>::const_iterator mapIt = map.begin();
            mapIt != map.end(); ++mapIt)
        {
            std::pair<Key, Value> entry = *mapIt;
            os << entry.first << " -> " << entry.second << std::endl;
        }
    }

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
    }
};

#endif /* _AFK_DATA_MAP_CACHE_H_ */

