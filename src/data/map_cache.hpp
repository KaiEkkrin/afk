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

#include <mutex>
#include <unordered_map>

#include "cache.hpp"

/* This defines the AFK cache as a simple std::unordered_map
 * guarded by a mutex.
 *
 * It's deprecated by the way.  I think I should probably just delete
 * it; I'm positive the Polymer is better for my purposes.
 */

template<class Key, class Value>
class AFK_MapCache: public AFK_Cache<Key, Value>
{
protected:
    std::unordered_map<Key, Value> map;
    std::mutex mut;

public:
    AFK_MapCache() {}
    virtual ~AFK_MapCache() {}

    virtual size_t size() const
    {
        return map.size();
    }

    virtual Value *get(unsigned int threadId, const Key& key)
    {
        std::unique_lock<std::mutex> lock(mut);
        return &map.at(key);
    }

    virtual Value *insert(unsigned int threadId, const Key& key)
    {
        std::unique_lock<std::mutex> lock(mut);
        return &map[key];
    }

#if 0
    virtual bool findDuplicate(const Key& key, const Value& value, Value& o_duplicateValue)
    {
        /* mapCache is uniquely keyed. */
        return false;
    }
#endif

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
    }
};

#endif /* _AFK_DATA_MAP_CACHE_H_ */

