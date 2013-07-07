/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_POLYMER_CACHE_H_
#define _AFK_DATA_POLYMER_CACHE_H_

#include "cache.hpp"
#include "polymer.hpp"

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
    AFK_PolymerCache(Hasher hasher): polymer(9, 4, hasher) {}
    virtual ~AFK_PolymerCache() {}

    virtual Value& operator[](const Key& key)
    {
        return polymer[key];
    }

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

