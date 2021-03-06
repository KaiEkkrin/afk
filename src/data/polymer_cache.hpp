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

#ifndef _AFK_DATA_POLYMER_CACHE_H_
#define _AFK_DATA_POLYMER_CACHE_H_

#include "cache.hpp"
#include "polymer.hpp"


/* Useful function for coming up with a bitness level to try. */
unsigned int afk_suggestCacheBitness(unsigned int entries);


/* This defines the AFK cache as an unguarded polymer cache. */

template<typename Key, typename Value, typename Hasher, const Key& unassigned, unsigned int hashBits, bool debug = false>
class AFK_PolymerCache: public AFK_Cache<Key, Value>
{
protected:
    AFK_Polymer<Key, Value, Hasher, unassigned, hashBits, debug> polymer;

public:
    AFK_PolymerCache(unsigned int targetContention, Hasher hasher):
        polymer(targetContention, hasher)
    {
    }

    virtual size_t size() const
    {
        return polymer.size();
    }

    virtual Value *get(unsigned int threadId, const Key& key)
    {
        return polymer.get(threadId, key);
    }

    virtual Value *insert(unsigned int threadId, const Key& key)
    {
        return polymer.insert(threadId, key);
    }

    /* I'll want to try this some day ? */
#if 0
    virtual bool findDuplicate(const Key& key, const Value& value, Value& o_duplicateValue)
    {
        Value& firstMatch = polymer[key];

        if (!(polymer[key] == value))
        {
            o_duplicateValue = firstMatch;
            return true;
        }

        return false;
    }
#endif

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
        polymer.printStats(os, prefix);
    }
};

#endif /* _AFK_DATA_POLYMER_CACHE_H_ */

