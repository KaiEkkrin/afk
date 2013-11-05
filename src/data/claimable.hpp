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

#ifndef _AFK_DATA_CLAIMABLE_H_
#define _AFK_DATA_CLAIMABLE_H_

#include <cassert>
#include <exception>
#include <functional>
#include <sstream>

#include <boost/atomic.hpp>
#include <boost/thread.hpp>

#include "frame.hpp"

/* A "Claimable" is a lockable thing with useful features. 
 * It can optionally
 * stamp the object with the frame time of use, and exclude
 * re-use until the next frame.
 */

/* This exception is thrown when you can't get something because
 * it's in use.  Try again.
 * Actual program errors are assert()ed instead.
 */
class AFK_ClaimException: public std::exception {};

/* How to specify a function to obtain the current computing frame */
typedef std::function<const AFK_Frame& (void)> AFK_GetComputingFrame;

template<typename T, AFK_GetComputingFrame& getComputingFrame>
class AFK_Claimable;

template<typename T, AFK_GetComputingFrame& getComputingFrame>
class AFK_Claim
{
private:
    AFK_Claim(const AFK_Claim& _c) { assert(false); }
    AFK_Claim& operator=(const AFK_Claim& _c) { assert(false); return *this; }

protected:
    unsigned int threadId; /* TODO change all thread IDs to 64-bit */
    AFK_Claimable<T, getComputingFrame> *claimable;
    bool shared;
    bool released;

    /* For sanity checking, for now I'm going to have the Claim
     * take an original of the object, as well as copy it to
     * give it to the caller to modify.
     * Upon release we can compare this with what's present to
     * make sure everything's working as expected
     * `obj' will always be valid (and will be the one modified
     * with a nonshared Claim); `original' will not be pulled
     * on a shared Claim.
     */
    T obj;
    T original;

    AFK_Claim(unsigned int _threadId, AFK_Claimable<T, getComputingFrame> *_claimable, bool _shared):
        threadId(_threadId), claimable(_claimable), shared(_shared), released(false),
        obj(_claimable->obj.load())
    {
        if (!shared) original = obj;
    }

public:
    virtual ~AFK_Claim() { if (!released) release(); }

    const T& getShared(void) const
    {
        assert(!released);
        return obj;
    }

    T& get(void) const
    {
        assert(!shared);
        assert(!released);
        return obj;
    }

    bool upgrade(void)
    {
        if (!shared) return true;

        if (claimable->tryUpgradeShared(threadId))
        {
            shared = false;
            original = obj;
            return true;
        }
        
        return false;
    }

    void release(void)
    {
        assert(!released);
        if (shared)
        {
            claimable->releaseShared(threadId);
        }
        else
        {
            assert(claimable->obj.compare_exchange_strong(original, obj));
            claimable->release(threadId);
        }

        released = true;
    }

    /* This function releases the claim without swapping the object
     * back, discarding any changes.
     */
    void invalidate(void)
    {
        assert(!released);
        if (shared) claimable->releaseShared(threadId);
        else claimable->release(threadId);
        released = true;
    }

    friend class AFK_Claimable<T, getComputingFrame>;
};

enum class AFK_ClaimFlags : int
{
    LOOP        = 1,
    EXCLUSIVE   = 2,
    EVICTOR     = 4,
    SHARED      = 8
};

/* TODO this is silly, remove the enum class and templates and just use
 * int with preprocessor :P
 */
template<AFK_ClaimFlags f>
constexpr AFK_ClaimFlags afk_claimFlagList(void) { return f; }

template<AFK_ClaimFlags f, AFK_ClaimFlags... n>
constexpr AFK_ClaimFlags afk_claimFlagList(void) { return ((int)f | (int)afk_claimFlagList<n...>()); }

#define AFK_TEST_CLAIM_FLAG(flags, f) ((int)(flags) & (int)AFK_ClaimFlags::f)

template<typename T, AFK_GetComputingFrame& getComputingFrame>
class AFK_Claimable
{
protected:
    /* Which thread ID has claimed use of the object.
     * The thread ID is incremented by 1 to make sure no 0s are
     * knocking about; 0 means unclaimed, and the top bit is the
     * non-shared flag.
     */
    boost::atomic<uint64_t> id;

    /* Last times the object was seen. */
    boost::atomic<int64_t> lastSeen;
    boost::atomic<int64_t> lastSeenExclusively;

    /* The claimable object itself. */
    boost::atomic<T> obj;

#define AFK_CL_NO_THREAD 0
#define AFK_CL_NONSHARED (1uLL<<63)

#define AFK_CL_THREAD_ID_SHARED(id) (((uint64_t)(id))+1uLL)
#define AFK_CL_THREAD_ID_SHARED_MASK(id) (~AFK_CL_THREAD_ID_SHARED(id))

#define AFK_CL_THREAD_ID_NONSHARED(id) (AFK_CL_THREAD_ID_SHARED(id) | AFK_CL_NONSHARED)
#define AFK_CL_THREAD_ID_NONSHARED_MASK(id) (~AFK_CL_THREAD_ID_NONSHARED(id))

    bool tryClaim(unsigned int threadId)
    {
        uint64_t expected = AFK_CL_NO_THREAD;
        return id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
    }

    bool tryClaimShared(unsigned int threadId)
    {
        if ((id.fetch_or(AFK_CL_THREAD_ID_SHARED(threadId)) & AFK_CL_NONSHARED) == AFK_CL_NONSHARED)
        {
            /* It's already claimed exclusively, flip that
             * bit back
             */
            id.fetch_and(AFK_CL_THREAD_ID_SHARED_MASK(threadId));
            return false;
        }

        return true;
    }

    bool tryUpgradeShared(unsigned int threadId)
    {
        uint64_t expected = AFK_CL_THREAD_ID_SHARED(threadId);
        return id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
    }

    void releaseShared(unsigned int threadId)
    {
        id.fetch_and(AFK_CL_THREAD_ID_SHARED_MASK(threadId));
    }

    void release(unsigned int threadId)
    {
        id.fetch_and(AFK_CL_THREAD_ID_NONSHARED_MASK(threadId));
    }

public:
    AFK_Claimable(): id(0), lastSeen(-1), lastSeenExclusively(-1), obj() {}

    /* Gets you a claim of the desired type.
     * The `exclusive' flag causes the `lastSeenExclusively'
     * field to be incremented if it's not already equal to
     * the current frame; this mechanism locks out an object
     * from being claimed more than once per frame.
     */
    AFK_Claim<T, getComputingFrame> claim(unsigned int threadId, AFK_ClaimFlags flags)
    {
        bool claimed = false;

        do
        {
            if (AFK_TEST_CLAIM_FLAG(flags, SHARED)) claimed = tryClaimShared(threadId);
            else claimed = tryClaim(threadId);
            if (!claimed && AFK_TEST_CLAIM_FLAG(flags, LOOP)) boost::this_thread::yield();
        }
        while (!claimed && AFK_TEST_CLAIM_FLAG(flags, LOOP));

        if (!claimed) throw AFK_ClaimException();

        int64_t computingFrameNum = getComputingFrame().get();

        /* Help the evictor out a little */
        if (AFK_TEST_CLAIM_FLAG(flags, EVICTOR))
        {
            assert(!AFK_TEST_CLAIM_FLAG(flags, SHARED));
            if (lastSeen.load() == computingFrameNum)
            {
                release(threadId);
                throw AFK_ClaimException();
            }
        }
        else
        {
            lastSeen.store(computingFrameNum);
        
            if ((int)flags & (int)AFK_ClaimFlags::EXCLUSIVE)
            {
                assert(!AFK_TEST_CLAIM_FLAG(flags, SHARED));
                if (lastSeenExclusively.exchange(computingFrameNum) == computingFrameNum)
                {
                    release(threadId);
                    throw AFK_ClaimException();
                }
            }
        }

        return AFK_Claim<T, getComputingFrame>(threadId, this, false);
    }

    int64_t getLastSeen(void) const { return lastSeen.load(); }
    int64_t getLastSeenExclusively(void) const { return lastSeenExclusively.load(); }

    friend class AFK_Claim<T, getComputingFrame>;

    template<typename _T, AFK_GetComputingFrame& _getComputingFrame>
    friend std::ostream& operator<<(std::ostream& os, const AFK_Claimable<_T, _getComputingFrame>& c);
};

template<typename T, AFK_GetComputingFrame& getComputingFrame>
std::ostream& operator<<(std::ostream& os, const AFK_Claimable<T, getComputingFrame>& c)
{
    /* I'm going to ignore any claims here and just read
     * out what's there -- it might be out of date, but
     * these functions are for diagnostics only and that
     * should be OK
     */
    os << "Claimable(" << c.obj.load() << ", last seen " << c.getLastSeen() << ", last seen exclusively " << c.getLastSeenExclusively() << ")";
    return os;
}

#endif /* _AFK_DATA_CLAIMABLE_H_ */

