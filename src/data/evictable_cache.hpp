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

#include <chrono>
#include <future>
#include <mutex>
#include <sstream>
#include <thread>

#include "cache.hpp"
#include "data.hpp"
#include "frame.hpp"
#include "polymer.hpp"

/* TODO: Interestingly enough, on Linux, volatile claimable seems to be
 * better, but on Windows, locked claimable is.  The opposite of what
 * I expected in fact!
 * Perhaps there's room to investigate using one for small things
 * (WorldCell) and another for big ones (LandscapeTile...)?
 */
#ifdef __GNUC__
#define AFK_EC_LOCKED_CLAIMABLE 0
#endif

#ifdef _WIN32
/* So: LockedClaimable itself works fine, but right now, because it makes
 * a mutex for every object we run out of mutexes and crash horribly
 * (it's not valid to have more than a million or so.)
 * The obvious TODO here, if I want to keep using LockedClaimable (and I
 * think I do) is to build a lock pool for it.
 */
#define AFK_EC_LOCKED_CLAIMABLE 0
#endif

#if AFK_EC_LOCKED_CLAIMABLE
#include "claimable_locked.hpp"
#define AFK_EVICTABLE_CLAIMABLE_TYPE(Value) AFK_LockedClaimable<Value>
#define AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value) AFK_LockedClaim<Value>
#define AFK_EVICTABLE_CLAIM_TYPE(Value) AFK_LockedClaim<Value>
#else
#include "claimable_volatile.hpp"
#define AFK_EVICTABLE_CLAIMABLE_TYPE(Value) AFK_VolatileClaimable<Value>
#define AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value) AFK_VolatileInplaceClaim<Value>
#define AFK_EVICTABLE_CLAIM_TYPE(Value) AFK_VolatileClaim<Value>
#endif

#include "watched_claimable.hpp"

/* An evictable cache is a polymer cache that can run an
 * eviction thread to remove old entries.  (It's not actually
 * based on PolymerCache, which remains merely as a test class.)
 * It uses the Evictable defined here as a monomer.
 * We require that the Value define the function:
 * - void evict(void) : evicts the entry.  should be OK to call multiple
 * times.  don't count on it always being the means for deletion.
 */

template<
    typename Value,
    int64_t framesBeforeEviction,
    AFK_GetComputingFrame& getComputingFrame>
class AFK_Evictable
{
public:
    AFK_WatchedClaimable<
        AFK_EVICTABLE_CLAIMABLE_TYPE(Value),
        AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value),
        AFK_EVICTABLE_CLAIM_TYPE(Value),
        getComputingFrame> claimable;

    AFK_Evictable(): claimable() {}

    bool canBeEvicted(void) const 
    {
        bool canEvict = ((getComputingFrame() - claimable.getLastSeen()) > framesBeforeEviction);
        return canEvict;
    }
};

template<
    typename Value,
    int64_t framesBeforeEviction,
    AFK_GetComputingFrame& getComputingFrame>
std::ostream& operator<<(
    std::ostream& os,
    const AFK_Evictable<Value, framesBeforeEviction, getComputingFrame>& ev)
{
    os << "Evictable(Value=" << ev.claimable << ")";
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
        AFK_Evictable<Value, framesBeforeEviction, getComputingFrame> >
{
public:
    typedef AFK_Evictable<Value, framesBeforeEviction, getComputingFrame> EvictableValue;
    typedef AFK_PolymerChain<Key, EvictableValue, unassigned, hashBits, debug> PolymerChain;

protected:
    class EvictableChainFactory
    {
    protected:
        /* There will be concurrent access to this object, it needs
         * to be guarded
         */
        std::mutex mut;

        /* We create new chains in a different thread in advance so
         * as to not stall the workers.
         */
        std::thread *th;
        std::promise<PolymerChain*> *rp;
        std::future<PolymerChain*> result;

        void kickoff(void)
        {
            rp = new std::promise<PolymerChain*>();
            th = new std::thread(
                &AFK_EvictableCache<Key, Value, Hasher, unassigned, hashBits, framesBeforeEviction, getComputingFrame, debug>::EvictableChainFactory::worker,
                this);
            result = rp->get_future();
        }

        PolymerChain *touchdown(void)
        {
            assert(result.valid());
            result.wait();
            PolymerChain *newChain = result.get();

            /* Clean things up, call kickoff() again to start the
             * next one
             */
            th->join();
            delete th; th = nullptr;
            delete rp; rp = nullptr;

            return newChain;
        }

        void worker(void) afk_noexcept
        {
            PolymerChain *newChain = new PolymerChain();
            for (size_t slot = 0; slot < CHAIN_SIZE; ++slot)
            {
                unsigned int threadId = 1; /* doesn't matter, no contention yet */
                Key key;
                EvictableValue *value;

                /* Make sure to sanity check this stuff.  The polymer needs
                 * to be initialising its monomer keys properly
                 */
                bool gotIt = newChain->atSlot(threadId, slot, true, &key, &value);
                assert(gotIt && key == unassigned);
                if (gotIt) value->claimable.claim(threadId, AFK_CL_LOOP).get() = Value();
            }

            rp->set_value(newChain);
        }

    public:
        EvictableChainFactory()
        {
            /* Kick off the task right away so there is a chain in reserve asap */
            kickoff();
        }

        virtual ~EvictableChainFactory()
        {
            std::unique_lock<std::mutex> lock(mut);
            touchdown();
        }

        PolymerChain *operator()()
        {
            std::unique_lock<std::mutex> lock(mut);
            PolymerChain *newChain = touchdown();

            /* Start making the next one right away so it's in reserve */
            kickoff();
            return newChain;
        }
    };

    AFK_Polymer<Key, EvictableValue, Hasher, unassigned, hashBits, debug, EvictableChainFactory> polymer;

    /* The state of the evictor. */
    const size_t targetSize;
    const size_t kickoffSize;
    const size_t complainSize;

    unsigned int threadId;
    std::thread *th;
    std::promise<unsigned int> *rp;
    std::future<unsigned int> result;

    bool stop;

    /* Stats. */
    unsigned int entriesEvicted;
    unsigned int runsSkipped;
    unsigned int runsOverlapped;

public:
    /* Use this to refer to claims of values in the cache. */
    typedef AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value) InplaceClaim;
    typedef AFK_EVICTABLE_CLAIM_TYPE(Value) Claim;

    void evictionWorker(void) afk_noexcept
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
                Key key;
                EvictableValue *candidate;
                if (this->polymer.getSlot(threadId, slot, &key, &candidate))
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
                        auto claim = candidate->claimable.claim(threadId, AFK_CL_EVICTOR);
                        if (claim.isValid())
                        {
                            if (candidate->canBeEvicted())
                            {
                                Value& obj = claim.get();
                                obj.evict();

                                /* Reset it: the polymer won't */
                                obj = Value();

                                if (!this->polymer.eraseSlot(threadId, slot, key))
                                {
                                    /* We'd better not release (and commit the reset value)
                                     * in this case!
                                     * (Which ought to be unlikely ...)
                                     */
                                    claim.invalidate();
                                }
                            }
                        }
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

    virtual EvictableValue *get(unsigned int threadId, const Key& key)
    {
        return polymer.get(threadId, key);
    }

    virtual EvictableValue *insert(unsigned int threadId, const Key& key)
    {
        return polymer.insert(threadId, key);
    }

    /* These helpers do the relevant sort of claim as well. */
    AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value) getAndClaimInplace(unsigned int threadId, const Key& key, unsigned int claimFlags)
    {
        EvictableValue *value = polymer.get(threadId, key);
        if (value) return value->claimable.claimInplace(threadId, claimFlags);
        else return AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value)();
    }

    AFK_EVICTABLE_CLAIM_TYPE(Value) getAndClaim(unsigned int threadId, const Key& key, unsigned int claimFlags)
    {
        EvictableValue *value = polymer.get(threadId, key);
        if (value) return value->claimable.claim(threadId, claimFlags);
        else return AFK_EVICTABLE_CLAIM_TYPE(Value)();
    }

    AFK_EVICTABLE_INPLACE_CLAIM_TYPE(Value) insertAndClaimInplace(unsigned int threadId, const Key& key, unsigned int claimFlags)
    {
        return polymer.insert(threadId, key)->claimable.claimInplace(threadId, claimFlags);
    }

    AFK_EVICTABLE_CLAIM_TYPE(Value) insertAndClaim(unsigned int threadId, const Key& key, unsigned int claimFlags)
    {
        return polymer.insert(threadId, key)->claimable.claim(threadId, claimFlags);
    }

    void doEvictionIfNecessary(void)
    {
        /* Check whether any current eviction task has finished */
        if (th && rp)
        {
            if (result.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
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
                rp = new std::promise<unsigned int>();
                th = new std::thread(
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

