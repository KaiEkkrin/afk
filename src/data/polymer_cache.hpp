/* AFK (c) Alex Holloway 2013 */

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

template<typename Key, typename Value, typename Hasher>
class AFK_PolymerCache: public AFK_Cache<Key, Value>
{
protected:
    AFK_Polymer<Key, Value, Hasher> polymer;

public:
    AFK_PolymerCache(unsigned int hashBits, unsigned int targetContention, Hasher hasher):
        polymer(hashBits, targetContention, hasher)
    {
    }

    virtual size_t size() const
    {
        return polymer.size();
    }

    virtual Value& at(const Key& key) const
    {
        return polymer.at(key);
    }

    virtual Value& operator[](const Key& key)
    {
        return polymer[key];
    }

    virtual bool erase(const Key& key)
    {
        return polymer.erase(key);
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

    virtual void printEverything(std::ostream& os) const
    {
        for (auto monoIt = polymer.begin(); monoIt != polymer.end(); ++monoIt)
        {
            AFK_Monomer<Key, Value>* monomer = monoIt->load();
            os << monomer->key << " -> " << monomer->value << std::endl;
        }
    }

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
        polymer.printStats(os, prefix);
    }
};

#endif /* _AFK_DATA_POLYMER_CACHE_H_ */

