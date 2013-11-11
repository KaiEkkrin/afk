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

#define AFK_DEBUG_CLAIMABLE 0

#if AFK_DEBUG_CLAIMABLE
#include "../debug.hpp"
#define AFK_DEBUG_PRINT_CLAIMABLE(expr) AFK_DEBUG_PRINT(expr)
#define AFK_DEBUG_PRINTL_CLAIMABLE(expr) AFK_DEBUG_PRINTL(expr)
#else
#define AFK_DEBUG_PRINT_CLAIMABLE(expr)
#define AFK_DEBUG_PRINTL_CLAIMABLE(expr)
#endif

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
    /* TODO: See below -- this isn't working, removing for now */
#define AFK_CL_COMPARE_ORIGINAL 0

    T obj;
#if AFK_CL_COMPARE_ORIGINAL
    T original;
#endif

    AFK_Claim(unsigned int _threadId, AFK_Claimable<T, getComputingFrame> *_claimable, bool _shared):
        threadId(_threadId), claimable(_claimable), shared(_shared), released(false),
        obj(_claimable->obj.load())
    {
#if AFK_CL_COMPARE_ORIGINAL
        if (!shared) original = obj;
#endif
    }

public:
    /* No reference counting is performed, so I must never copy a
     * claim around
     */
    AFK_Claim(const AFK_Claim& _c) = delete;
    AFK_Claim& operator=(const AFK_Claim& _c) = delete;

    /* I need a move constructor in order to return a claim from
     * the claimable's claim method.  (Although for whatever
     * reason, this and the move assign don't seem to get called
     * when acquiring world cell and landscape tile claims.
     * Hmm.
     * ...I wonder how prone to creating bugs this might be...
     */ 
    AFK_Claim(AFK_Claim&& _claim):
        threadId(_claim.threadId), claimable(_claim.claimable), shared(_claim.shared), released(_claim.released),
        obj(_claim.obj)
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving claim for " << std::hex << claimable << ": " << obj)
        _claim.released = true;
    }

    /* ...likewise... */
    AFK_Claim& operator=(AFK_Claim&& _claim)
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("assign moving claim for " << std::hex << claimable << ": " << obj)

        threadId        = _claim.threadId;
        claimable       = _claim.claimable;
        shared          = _claim.shared;
        released        = _claim.released;
        obj             = _claim.obj;
        _claim.released = true;
        return *this;
    }

    virtual ~AFK_Claim()
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing claim for " << std::hex << claimable << ": " << obj << "(released: " << released << ")")
        if (!released) release();
     }

    const T& getShared(void) const
    {
        assert(!released);
        return obj;
    }

    T& get(void)
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
#if AFK_CL_COMPARE_ORIGINAL
            original = obj;
#endif
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
            /* TODO: I've verified, below, that this comparison
             * (that I wanted to perform) does not work.  :(
             * Bit-identical objects compare as different.
             * Therefore, I'm going to do store() for now and
             * comment out the `original' stuff, and lose a
             * valuable sanity check :(
             */
            //assert(claimable->obj.compare_exchange_strong(original, obj));
#if AFK_CL_COMPARE_ORIGINAL
            if (!claimable->obj.compare_exchange_strong(original, obj))
            {
                T inplace = claimable->obj.load();
                AFK_DEBUG_PRINTL_CLAIMABLE("release failed: original " << original << ", inplace " << inplace)
                assert(false);
            }
#else
            claimable->obj.store(obj);
#endif
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

#define AFK_CL_LOOP         1
#define AFK_CL_EXCLUSIVE    2
#define AFK_CL_EVICTOR      4
#define AFK_CL_SHARED       8

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

    /* The move constructors are used to enable initialisation.
     * They essentially make a new Claimable.
     */
    AFK_Claimable(const AFK_Claimable&& _claimable):
        id(0), lastSeen(-1), lastSeenExclusively(-1)
    {
        obj.store(_claimable.obj.load());
    }

    AFK_Claimable& operator=(const AFK_Claimable&& _claimable)
    {
        id.store(0);
        lastSeen.store(-1);
        lastSeenExclusively.store(-1);
        obj.store(_claimable.obj.load());
        return *this;
    }

    /* Gets you a claim of the desired type.
     * The `exclusive' flag causes the `lastSeenExclusively'
     * field to be incremented if it's not already equal to
     * the current frame; this mechanism locks out an object
     * from being claimed more than once per frame.
     */
    AFK_Claim<T, getComputingFrame> claim(unsigned int threadId, unsigned int flags) 
    {
        bool claimed = false;

        do
        {
            if (flags & AFK_CL_SHARED) claimed = tryClaimShared(threadId);
            else claimed = tryClaim(threadId);
            if (flags & AFK_CL_LOOP) boost::this_thread::yield();
        }
        while (!claimed && (flags & AFK_CL_LOOP));

        if (!claimed) throw AFK_ClaimException();

        int64_t computingFrameNum = getComputingFrame().get();

        /* Help the evictor out a little */
        if (flags & AFK_CL_EVICTOR)
        {
            assert(!(flags & AFK_CL_SHARED));
            if (lastSeen.load() == computingFrameNum)
            {
                release(threadId);
                throw AFK_ClaimException();
            }
        }
        else
        {
            lastSeen.store(computingFrameNum);
        
            if (flags & AFK_CL_EXCLUSIVE)
            {
                assert(!(flags & AFK_CL_SHARED));
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

