/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_POLYMER_CACHE_H_
#define _AFK_DATA_POLYMER_CACHE_H_

#include "cache.hpp"
#include "polymer.hpp"

#define AFK_POLYMER_HASHBITS 20
#define AFK_POLYMER_CONTENTION 3

/* This defines the AFK cache as an unguarded polymer cache. */

template<typename Key, typename Value, typename Hasher>
class AFK_PolymerCache: public AFK_Cache<Key, Value>
{
protected:
    AFK_Polymer<Key, Value, Hasher> polymer;

public:
    /* TODO: These values should probably be tweakable from elsewhere
     * (if I'm going to use this structure at all.  Maybe mutexes
     * are really fast now?
     */
    AFK_PolymerCache(Hasher hasher): polymer(AFK_POLYMER_HASHBITS, AFK_POLYMER_CONTENTION, hasher) {}
    virtual ~AFK_PolymerCache() {}

    virtual Value& operator[](const Key& key)
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

    virtual void printEverything(std::ostream& os) const
    {
        for (typename AFK_PolymerChain<Key, Value>::iterator polyIt = polymer.begin();
            polyIt != polymer.end(); ++polyIt)
        {
            boost::atomic<AFK_Monomer<Key, Value>*>& monoAt = *polyIt;
            AFK_Monomer<Key, Value>* monomer = monoAt.load();
            os << monomer->key << " -> " << monomer->value << std::endl;
        }
    }

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
        return polymer.printStats(os, prefix);
    }
};

#endif /* _AFK_DATA_POLYMER_CACHE_H_ */

