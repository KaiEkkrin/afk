/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_EVICTABLE_CACHE_H_
#define _AFK_DATA_EVICTABLE_CACHE_H_

#include <sstream>

#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>

#include "claimable.hpp"
#include "polymer_cache.hpp"

/* An evictable cache is a polymer cache that can run an
 * eviction thread to remove old entries.
 * It assumes that the Value is a Claimable.
 */

template<typename Key, typename Value, typename Hasher>
class AFK_EvictableCache: public AFK_PolymerCache<Key, Value, Hasher>
{
protected:
    /* The state of the evictor. */
    const size_t targetSize;
    const size_t kickoffSize;
    const size_t complainSize;

    unsigned int threadId;
    boost::thread *th;
    boost::promise<unsigned int> *rp;
    boost::unique_future<unsigned int> result;

    bool stop;

    /* Stats. */
    unsigned int entriesEvicted;
    unsigned int runsSkipped;
    unsigned int runsOverlapped;

    void evictionWorker(void)
    {
        unsigned int entriesEvicted = 0;
        std::vector<AFK_Monomer<Key, Value>*> forRemoval;

        do
        {
            /* TODO Does this walk need randomising?  Maybe I should check
             * for eviction artifacts before I included something complicated
             * including an RNG
             */
            size_t slotCount = this->polymer.slotCount(); /* don't keep recomputing */
            for (size_t slot = 0; slot < slotCount; ++slot)
            {
                AFK_Monomer<Key, Value> *candidate = this->polymer.atSlot(slot);
                if (candidate && candidate->value.canBeEvicted())
                {
                    /* Claim it first, otherwise someone else will
                     * and the world will not be a happy place.
                     * If someone else has it, chances are it's
                     * needed after all and I should let go!
                     */
                    if (candidate->value.claim(threadId, AFK_CLT_EVICTOR) == AFK_CL_CLAIMED)
                    {
                        bool deleted = false;

                        if (this->polymer.eraseSlot(slot, candidate))
                        {
                            /* Double check */
                            if (candidate->value.canBeEvicted())
                            {
                                delete candidate;
                                ++entriesEvicted;
                                deleted = true;
                            }
                            else
                            {
                                /* Uh oh.  Quick, put it back. */
                                this->polymer.reinsertSlot(slot, candidate);
                            }
                        }
    
                        if (!deleted) candidate->value.release(threadId);
                    }
                }
            }
        } while (!stop && this->polymer.size() > targetSize);

        rp->set_value(entriesEvicted);
    }

public:
    AFK_EvictableCache(
        unsigned int hashBits,
        unsigned int targetContention,
        Hasher hasher,
        size_t _targetSize,
        unsigned int _threadId):
            AFK_PolymerCache<Key, Value, Hasher>(hashBits, targetContention, hasher),
            targetSize(_targetSize),
            kickoffSize(_targetSize + _targetSize / 4),
            complainSize(_targetSize + _targetSize / 2),
            threadId(_threadId),
            th(NULL), rp(NULL), stop(false),
            entriesEvicted(0), runsSkipped(0), runsOverlapped(0)
    {
    }

    virtual ~AFK_EvictableCache()
    {
        if (th)
        {
            stop = true;
            th->join();
            delete th;
        }

        if (rp) delete rp;
    }

    void doEvictionIfNecessary(void)
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
            if (this->polymer.size() > kickoffSize)
            {
                /* Kick off a new eviction task */
                rp = new boost::promise<unsigned int>();
                th = new boost::thread(&AFK_EvictableCache<Key, Value, Hasher>::evictionWorker, this);
                result = rp->get_future();
            }
            else
            {
                ++runsSkipped;
            }
        }
    }

    virtual void printStats(std::ostream& os, const std::string& prefix) const
    {
        AFK_PolymerCache<Key, Value, Hasher>::printStats(os, prefix);
        /* TODO An eviction rate would be much more interesting */
        os << prefix << ": Evicted " << entriesEvicted << " entries" << std::endl;
        os << prefix << ": " << runsOverlapped << " runs overlapped, " << runsSkipped << " runs skipped" << std::endl;
    }

    bool withinTargetSize(void) const
    {
        return this->size() < targetSize;
    }

    bool wayOutsideTargetSize(void) const
    {
        return this->size() > complainSize;
    }
};

#endif /* _AFK_DATA_EVICTABLE_CACHE_H_ */

