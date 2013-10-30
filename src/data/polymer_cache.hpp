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

#ifndef _AFK_DATA_POLYMER_CACHE_H_
#define _AFK_DATA_POLYMER_CACHE_H_

#include <boost/atomic.hpp>
#include <boost/ref.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>

#include "cache.hpp"
#include "polymer.hpp"


/* Useful function for coming up with a bitness level to try. */
unsigned int afk_suggestCacheBitness(unsigned int entries);


/* This defines the AFK cache as an unguarded polymer cache. */

template<typename Key, typename Monomer, typename Hasher>
class AFK_PolymerCache: public AFK_Cache<Key, Monomer>
{
protected:
    AFK_Polymer<Key, Monomer, Hasher> polymer;

public:
    AFK_PolymerCache(const Key& unassigned, unsigned int hashBits, unsigned int targetContention, Hasher hasher):
        polymer(unassigned, hashBits, targetContention, hasher)
    {
    }

    virtual size_t size() const
    {
        return polymer.size();
    }

    virtual Monomer& at(const Key& key) const
    {
        return polymer.at(key);
    }

    virtual Monomer& operator[](const Key& key)
    {
        return polymer[key];
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

