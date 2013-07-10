/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_POLYMER_CACHE_H_
#define _AFK_DATA_POLYMER_CACHE_H_

#include <boost/atomic.hpp>
#include <boost/function.hpp>
#include <boost/ref.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>

#include "cache.hpp"
#include "polymer.hpp"

#define AFK_POLYMER_HASHBITS 20
#define AFK_POLYMER_CONTENTION 4

/* This object wraps a thread that evicts things from the polymer
 * cache when it gets too big.
 * TODO: Multiple threads ?
 */
template<typename Key, typename Value, typename Hasher>
class AFK_PolymerCacheEvictor
{
protected:
    AFK_Polymer<Key, Value, Hasher> *polymer; /* we don't own this */
    size_t targetSize;
    boost::function<bool (const Value&)> canBeEvicted;
    size_t kickoffSize;

    boost::thread *th;
    boost::promise<unsigned int> *rp;
    boost::unique_future<unsigned int> result;

    /* Stats. */
    unsigned int entriesEvicted;
    unsigned int runsSkipped;
    unsigned int runsOverlapped;

    void evictionWorker()
    {
        unsigned int entriesEvicted = 0;

        /* TODO Right now this is ruining all the parameters.  I think that
         * there might be a bug with templated parameters in boost::thread(),
         * and I should try making a specialised one and invoking that. :(
         */
        do
        {
            /* TODO Does this walk need randomising?  Maybe I should check
             * for eviction artifacts before I included something complicated
             * including an RNG
             */
            size_t slotCount = polymer->slotCount(); /* don't keep recomputing */
            for (size_t slot = 0; slot < slotCount; ++slot)
            {
                AFK_Monomer<Key, Value> *candidate = polymer->atSlot(slot);
                if (candidate && canBeEvicted(candidate->value))
                    //if (polymer.eraseSlot(slot, candidate)) ++entriesEvicted;
                    ++entriesEvicted;
            }
        } while (polymer->size() > targetSize);

        rp->set_value(entriesEvicted);
    }

public:
    AFK_PolymerCacheEvictor(AFK_Polymer<Key, Value, Hasher> *polymer,
        size_t _targetSize, boost::function<bool (const Value&)> _canBeEvicted):
            targetSize (_targetSize), canBeEvicted (_canBeEvicted),
            kickoffSize (_targetSize + _targetSize / 4), /* Tweakable */
            th (NULL), rp (NULL),
            entriesEvicted (0), runsSkipped (0), runsOverlapped (0)
    {
    }

    virtual ~AFK_PolymerCacheEvictor()
    {
        if (th)
        {
            th->join();
            delete th;
        }

        if (rp) delete rp;
    }

    void doEvictionIfNecessary()
    {
        /* Check whether any current eviction task has finished */
        if (th && rp)
        {
            if (result.is_ready())
            {
                entriesEvicted += result.get();
                th->join();
                delete th; th = NULL;
                delete rp; rp = NULL;
            }
        }

        if (th)
        {
            ++runsOverlapped;
        }
        else
        {
            if (polymer->size() > kickoffSize)
            {
                /* Kick off a new eviction task */
                rp = new boost::promise<unsigned int>();
                th = new boost::thread(&AFK_PolymerCacheEvictor<Key, Value, Hasher>::evictionWorker, this);
                //th = new boost::thread(afk_evictionWorker<Key, Value, Hasher>,
                //    boost::ref(polymer), targetSize, canBeEvicted, boost::ref(rp)); 
                result = rp->get_future();
            }
            else
            {
                ++runsSkipped;
            }
        }
    }

    void printStats(std::ostream& os, const std::string& prefix) const
    {
        /* TODO An eviction rate would be much more interesting */
        os << prefix << ": Evicted " << entriesEvicted << " entries" << std::endl;
        os << prefix << ": " << runsOverlapped << " runs overlapped, " << runsSkipped << " runs skipped" << std::endl;
    }
};

/* This defines the AFK cache as an unguarded polymer cache. */

template<typename Key, typename Value, typename Hasher>
class AFK_PolymerCache: public AFK_Cache<Key, Value>
{
protected:
    AFK_Polymer<Key, Value, Hasher> polymer;
    AFK_PolymerCacheEvictor<Key, Value, Hasher> *evictor;

public:
    /* TODO: These values should probably be tweakable from elsewhere
     * (if I'm going to use this structure at all.  Maybe mutexes
     * are really fast now?
     */
    AFK_PolymerCache(Hasher hasher, size_t targetSize, boost::function<bool (const Value&)> canBeEvicted):
        polymer(AFK_POLYMER_HASHBITS, AFK_POLYMER_CONTENTION, hasher)
    {
        evictor = new AFK_PolymerCacheEvictor<Key, Value, Hasher>(&polymer, targetSize, canBeEvicted);
    }

    virtual ~AFK_PolymerCache()
    {
        delete evictor;
    }

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

    virtual void doEvictionIfNecessary(void)
    {
        evictor->doEvictionIfNecessary();
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
        polymer.printStats(os, prefix);
        evictor->printStats(os, prefix);
    }
};

#endif /* _AFK_DATA_POLYMER_CACHE_H_ */

