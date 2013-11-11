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

#ifndef _AFK_DATA_EVICTABLE_CACHE_H_
#define _AFK_DATA_EVICTABLE_CACHE_H_

#include <sstream>

#include <boost/thread/future.hpp>
#include <boost/thread/thread.hpp>

#include "cache.hpp"
#include "claimable.hpp"
#include "frame.hpp"
#include "polymer.hpp"

/* An evictable cache is a polymer cache that can run an
 * eviction thread to remove old entries.  (It's not actually
 * based on PolymerCache, which remains merely as a test class.)
 * It uses the Evictable defined here as a monomer.
 * We require that the Value define the function:
 * - void evict(void) : evicts the entry.  should be OK to call multiple
 * times.  don't count on it always being the means for deletion.
 */

template<
    typename Key,
    typename Value,
    const Key& unassigned,
    int64_t framesBeforeEviction,
    AFK_GetComputingFrame& getComputingFrame>
class AFK_Evictable
{
public:
    boost::atomic<Key> key;
    AFK_Claimable<Value, getComputingFrame> claimable;

    AFK_Evictable(): key(unassigned), claimable() {}

    bool canBeEvicted(void) const 
    {
        bool canEvict = ((getComputingFrame() - claimable.getLastSeen()) > framesBeforeEviction);
        return canEvict;
    }
};

template<
    typename Key,
    typename Value,
    const Key& unassigned,
    int64_t framesBeforeEviction,
    AFK_GetComputingFrame& getComputingFrame>
std::ostream& operator<<(
    std::ostream& os,
    const AFK_Evictable<Key, Value, unassigned, framesBeforeEviction, getComputingFrame>& ev)
{
    os << "Evictable(Key=" << ev.key.load() << ", Value=" << ev.claimable << ")";
    return os;
}

template<
    typename Key,
    typename Value,
    typename Hasher,
    const Key& unassigned,
    unsigned int hashBits,
    int64_t framesBeforeEviction,
    AFK_GetComputingFrame& getComputingFrame,
    bool debug = false>
class AFK_EvictableCache:
    public AFK_Cache<
        Key,
        AFK_Evictable<Key, Value, unassigned, framesBeforeEviction, getComputingFrame> >
{
protected:
    typedef AFK_Evictable<Key, Value, unassigned, framesBeforeEviction, getComputingFrame> Monomer;
    typedef AFK_PolymerChain<Key, Monomer, unassigned, hashBits, debug> PolymerChain;

    class EvictableChainFactory
    {
    public:
        PolymerChain *operator()() const
        {
            PolymerChain *newChain = new PolymerChain();
            for (size_t slot = 0; slot < CHAIN_SIZE; ++slot)
            {
                Monomer *monomer;
                assert(newChain->atSlot(slot, &monomer));
                monomer->claimable.claim(1 /* doesn't matter, no contention */, AFK_CL_LOOP).get() = Value();
            }

            return newChain;
        }
    };

    AFK_Polymer<Key, Monomer, Hasher, unassigned, hashBits, debug, EvictableChainFactory> polymer;

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

public:
    void evictionWorker(void)
    {
        unsigned int entriesEvicted = 0;

        do
        {
            /* TODO Does this walk need randomising?  Maybe I should check
             * for eviction artifacts before I included something complicated
             * including an RNG
             */
            size_t slotCount = this->polymer.slotCount(); /* don't keep recomputing */
            for (size_t slot = 0; slot < slotCount; ++slot)
            {
                Monomer *candidate;
                if (this->polymer.getSlot(slot, &candidate))
                {
                    if (candidate->canBeEvicted())
                    {
                        /* Claim it first, otherwise someone else will
                         * and the world will not be a happy place.
                         * If someone else has it, chances are it's
                         * needed after all and I should let go!
                         * Note that the evictor claim type never uses
                         * a real frame number and so the following is OK
                         */
                        try
                        {
                            auto claim = candidate->claimable.claim(threadId, AFK_CL_EVICTOR);
                            if (candidate->canBeEvicted())
                            {
                                Value& obj = claim.get();
                                obj.evict();

                                /* Reset it: the polymer won't */
                                obj = Value();

                                if (!this->polymer.eraseSlot(slot, candidate->key))
                                {
                                    /* We'd better not release (and commit the reset value)
                                     * in this case!
                                     * (Which ought to be unlikely ...)
                                     */
                                    claim.invalidate();
                                }
                            }
                        }
                        catch (AFK_ClaimException) {}
                    }
                }
            }
        } while (!stop && this->polymer.size() > targetSize);

        rp->set_value(entriesEvicted);
    }

    AFK_EvictableCache(
        unsigned int targetContention,
        Hasher hasher,
        size_t _targetSize,
        unsigned int _threadId):
            polymer(targetContention, hasher),
            targetSize(_targetSize),
            kickoffSize(_targetSize + _targetSize / 4),
            complainSize(_targetSize + _targetSize / 2),
            threadId(_threadId),
            th(nullptr), rp(nullptr), stop(false),
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

    virtual size_t size() const
    {
        return polymer.size();
    }

    virtual Monomer& at(const Key& key)
    {
        Monomer *ptr = polymer.get(key);
        return *ptr;
    }

    virtual Monomer& operator[](const Key& key)
    {
        Monomer *ptr = polymer.insert(key);
        return *ptr;
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
                delete th; th = nullptr;
                delete rp; rp = nullptr;
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
                th = new boost::thread(
                    &AFK_EvictableCache<Key, Value, Hasher, unassigned, hashBits, framesBeforeEviction, getComputingFrame, debug>::evictionWorker,
                    this);
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
        polymer.printStats(os, prefix);
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

