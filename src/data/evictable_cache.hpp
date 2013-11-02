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

#include "claimable.hpp"
#include "frame.hpp"
#include "polymer_cache.hpp"

/* An evictable cache is a polymer cache that can run an
 * eviction thread to remove old entries.
 * It assumes that the Monomer has:
 * - a Claimable field named `claimable';
 * - a method bool canBeEvicted(void) const, that tells it
 * whether it's evictable
 * - a method void evict(void) that cleans it out.
 */

template<typename Key, typename Monomer, typename Hasher, const Key& unassigned, bool debug = false>
class AFK_EvictableCache: public AFK_PolymerCache<Key, Monomer, Hasher, unassigned, debug>
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
                    /* Claim it first, otherwise someone else will
                     * and the world will not be a happy place.
                     * If someone else has it, chances are it's
                     * needed after all and I should let go!
                     * Note that the evictor claim type never uses
                     * a real frame number and so the following is OK
                     */
                    if (candidate->claimable.claim(threadId, AFK_CLT_EVICTOR, AFK_Frame()) == AFK_CL_CLAIMED)
                    {
                        if (candidate->canBeEvicted() &&
                            this->polymer.eraseSlot(slot, candidate->key.load()))
                        {
                            candidate->evict();
                            ++entriesEvicted;
                        }

                        candidate->claimable.release(threadId, AFK_CL_CLAIMED);
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
            AFK_PolymerCache<Key, Monomer, Hasher, unassigned, debug>(hashBits, targetContention, hasher),
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
                th = new boost::thread(&AFK_EvictableCache<Key, Monomer, Hasher, unassigned, debug>::evictionWorker, this);
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
        AFK_PolymerCache<Key, Monomer, Hasher, unassigned, debug>::printStats(os, prefix);
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

