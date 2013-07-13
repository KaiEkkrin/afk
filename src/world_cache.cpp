/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "world_cache.hpp"


#define EVICTION_THREAD_ID 0xfffffffeu

void AFK_WorldCache::evictionWorker()
{
    unsigned int entriesEvicted = 0;
    std::vector<AFK_Monomer<AFK_Cell, AFK_WorldCell>*> forRemoval;

    do
    {
        /* TODO Does this walk need randomising?  Maybe I should check
         * for eviction artifacts before I included something complicated
         * including an RNG
         */
        size_t slotCount = polymer.slotCount(); /* don't keep recomputing */
        for (size_t slot = 0; slot < slotCount; ++slot)
        {
            AFK_Monomer<AFK_Cell, AFK_WorldCell> *candidate = polymer.atSlot(slot);
            if (candidate && candidate->value.canBeEvicted())
            {
                /* Claim it first, otherwise someone else will
                 * and the world will not be a happy place.
                 * If someone else has it, chances are it's
                 * needed after all and I should let go!
                 */
                if (candidate->value.claim(EVICTION_THREAD_ID, false) == AFK_WCC_CLAIMED)
                {
                    bool deleted = false;

                    if (polymer.eraseSlot(slot, candidate))
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
                            polymer.reinsertSlot(slot, candidate);
                        }
                    }

                    if (!deleted) candidate->value.release(EVICTION_THREAD_ID);
                }
            }
        }
    } while (!stop && polymer.size() > targetSize);

    rp->set_value(entriesEvicted);
}

AFK_WorldCache::AFK_WorldCache(size_t _targetSize):
    AFK_PolymerCache<AFK_Cell, AFK_WorldCell, AFK_HashCell>(AFK_HashCell()),
    targetSize(_targetSize),
    kickoffSize(_targetSize + _targetSize / 4),
    th(NULL), rp(NULL), stop(false),
    entriesEvicted(0), runsSkipped(0), runsOverlapped(0)
{
}

AFK_WorldCache::~AFK_WorldCache()
{
    if (th)
    {
        stop = true;
        th->join();
        delete th;
    }

    if (rp) delete rp;
}

void AFK_WorldCache::doEvictionIfNecessary()
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
        if (polymer.size() > kickoffSize)
        {
            /* Kick off a new eviction task */
            rp = new boost::promise<unsigned int>();
            th = new boost::thread(&AFK_WorldCache::evictionWorker, this);
            result = rp->get_future();
        }
        else
        {
            ++runsSkipped;
        }
    }
}

void AFK_WorldCache::printStats(std::ostream& os, const std::string& prefix) const
{
    AFK_PolymerCache<AFK_Cell, AFK_WorldCell, AFK_HashCell>::printStats(os, prefix);
    /* TODO An eviction rate would be much more interesting */
    os << prefix << ": Evicted " << entriesEvicted << " entries" << std::endl;
    os << prefix << ": " << runsOverlapped << " runs overlapped, " << runsSkipped << " runs skipped" << std::endl;
}

bool AFK_WorldCache::withinTargetSize(void) const
{
    return size() < targetSize;
}
