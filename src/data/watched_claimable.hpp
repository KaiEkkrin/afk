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

#ifndef _AFK_DATA_WATCHED_CLAIMABLE_H_
#define _AFK_DATA_WATCHED_CLAIMABLE_H_

#include "claimable.hpp"

/* A WatchedClaimable wraps a Claimable and provides last seen,
 * per-frame exclusivity, and an exception for the evictor.
 * It's generic, so that it can wrap a locked or volatile claimable.
 */

template<
    typename Claimable,
    typename InplaceClaim,
    typename Claim,
    AFK_GetComputingFrame& getComputingFrame>
class AFK_WatchedClaimable
{
protected:
    Claimable claimable;

    /* Last times the object was seen. */
    boost::atomic_uint_fast64_t lastSeen;
    boost::atomic_uint_fast64_t lastSeenExclusively;

    /* This utility method verifies that it's okay to claim ths object,
     * based on the last seen fields and the given flags.
     */
    bool watch(unsigned int flags) noexcept
    {
        int64_t computingFrameNum = getComputingFrame().get();

        /* Help the evictor out a little */
        if (flags & AFK_CL_EVICTOR)
        {
            assert(!(AFK_CL_IS_SHARED(flags)));
            if (lastSeen.load() == computingFrameNum) return false;
        }
        else
        {
            lastSeen.store(computingFrameNum);
        
            if (flags & AFK_CL_EXCLUSIVE)
            {
                assert(!(AFK_CL_IS_SHARED(flags)));
                if (lastSeenExclusively.exchange(computingFrameNum) == computingFrameNum) return false;
            }
        }

        return true;
    }

public:
    AFK_WatchedClaimable(): claimable(), lastSeen(-1), lastSeenExclusively(-1) {}

    /* The move constructors are used to enable initialisation.
     * They essentially make a new Claimable.
     */
    AFK_WatchedClaimable(const AFK_WatchedClaimable&& _wc):
        claimable(_wc.claimable), lastSeen(-1), lastSeenExclusively(-1) {}

    AFK_WatchedClaimable& operator=(const AFK_WatchedClaimable&& _wc)
    {
        claimable = _wc.claimable;
        lastSeen.store(-1);
        lastSeenExclusively.store(-1);
        return *this;
    }

    Claim claim(unsigned int threadId, unsigned int flags)
    {
        bool claimed = claimable.claimInternal(threadId, flags);
        if (!claimed) throw AFK_ClaimException();
        if (!watch(flags))
        {
            claimable.release(threadId);
            throw AFK_ClaimException();
        }

        return claimable.getClaim(threadId, flags);
    }

    InplaceClaim claimInplace(unsigned int threadId, unsigned int flags)
    {
        bool claimed = claimable.claimInternal(threadId, flags);
        if (!claimed) throw AFK_ClaimException();
        if (!watch(flags))
        {
            claimable.release(threadId);
            throw AFK_ClaimException();
        }

        return claimable.getInplaceClaim(threadId, flags);
    }

    int64_t getLastSeen(void) const { return lastSeen.load(); }
    int64_t getLastSeenExclusively(void) const { return lastSeenExclusively.load(); }

    template<
        typename _Claimable,
        typename _InplaceClaim,
        typename _Claim,
        AFK_GetComputingFrame& _getComputingFrame>
    friend std::ostream& operator<<(
        std::ostream& os,
        const AFK_WatchedClaimable<_Claimable, _InplaceClaim, _Claim, _getComputingFrame>& c);
};

template<
    typename Claimable,
    typename InplaceClaim,
    typename Claim,
    AFK_GetComputingFrame& getComputingFrame>
std::ostream& operator<<(
    std::ostream& os,
    const AFK_WatchedClaimable<Claimable, InplaceClaim, Claim, getComputingFrame>& c)
{
    os << "WatchedClaimable(with " << c.claimable << ", last seen " << c.getLastSeen() << ", last seen exclusively " << c.getLastSeenExclusively() << ")";
    return os;
}

#endif /* _AFK_DATA_WATCHED_CLAIMABLE_H_ */

