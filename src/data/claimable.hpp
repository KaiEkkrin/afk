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
#include <sstream>

#include <boost/atomic.hpp>
#include <boost/functional.hpp>

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
typedef boost::function<const AFK_Frame& (void)> AFK_GetComputingFrame;

template<typename T, AFK_GetComputingFrame getComputingFrame>
class AFK_Claimable<T, getComputingFrame>;

template<typename T>
class AFK_Claim
{
private:
    AFK_Claim(const AFK_Claim& _c) { assert(false); }
    AFK_Claim& operator=(const AFK_Claim& _c) { assert(false); return *this; }

protected:
    unsigned int threadId;
    AFK_Claimable<T> *claimable;
    bool released;

    /* For sanity checking, for now I'm going to have the Claim
     * take an original of the object, as well as copy it to
     * give it to the caller to modify.
     * Upon release we can compare this with what's present to
     * make sure everything's working as expected
     */
    T original;
    T obj;

    AFK_Claim(unsigned int _threadId, AFK_Claimable<T> *_claimable):
        threadId(_threadId), claimable(_claimable), released(false),
        original(_claimable->obj), obj(_claimable->obj)
    {
    }

public:
    virtual ~AFK_Claim() { if (!released) release(); }

    T& get(void) const
    {
        assert(!released);
        return claimable->obj;
    }

    void release(void)
    {
        assert(!released);
        assert(claimable->obj.compare_exchange_strong(original, obj));
        claimable->release(threadId);
        released = true;
    }

    /* This function releases the claim without swapping the object
     * back, discarding any changes.
     */
    void invalidate(void)
    {
        assert(!released);
        claimable->release(threadId);
        released = true;
    }

    template<typename T, AFK_GetComputingFrame getComputingFrame>
    friend class AFK_Claimable<T, getComputingFrame>;
}

template<typename T>
class AFK_SharedClaim
{
private:
    AFK_SharedClaim(const AFK_SharedClaim& _c) { assert(false); }
    AFK_SharedClaim& operator=(const AFK_SharedClaim& _c) { assert(false); return *this; }

protected:
    unsigned int threadId; /* TODO change all thread IDs to 64-bit */
    AFK_Claimable<T> *claimable;
    bool released;

    AFK_SharedClaim(unsigned int _threadId, AFK_Claimable<T> *_claimable):
        threadId(_threadId), claimable(_claimable), released = false
    {
    }

public:
    virtual ~AFK_SharedClaim() { if (!released) release(); }

    const T& get(void) const
    {
        assert(!released);
        return claimable->obj;
    }

    AFK_Claim<T> upgrade(void)
    {
        if (claimable->tryUpgradeShared(threadId))
        {
            /* Invalidate this shared claim, the claimable id is
             * no longer the one the release method expects
             */
            released = true;

            return AFK_Claim<T>(threadId, claimable);
        }
        else throw AFK_ClaimException();
    }

    void release(void)
    {
        assert(!released);
        claimable->releaseShared(threadId);
        released = true;
    }

    template<typename T, GetComputingFrame getComputingFrame>
    friend class AFK_Claimable<T, getComputingFrame>;
};

enum class AFK_ClaimFlags : int
{
    LOOP        = 1,
    EXCLUSIVE   = 2,
    EVICTOR     = 4
};

template<typename T, GetComputingFrame getComputingFrame>
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

    bool tryUpgradeShared(unsigned int threadId)
    {
        uint64_t expected = AFK_CL_THREAD_ID_SHARED(shared.threadId);
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

    /* Upgrades your shared claim to an unshared one. */
    AFK_Claim upgrade(AFK_SharedClaim& shared)
    {
        uint64_t expected = AFK_CL_THREAD_ID_SHARED(shared.threadId);
        if (id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId)))
        {
            /* Invalidate that old shared claim */
            shared.released = true;

            return AFK_Claim<T>(shared.threadId, this);
        }

        throw AFK_ClaimException();
    }

public:
    AFK_Claimable(): id(0), lastSeen(-1), lastSeenExclusively(-1), obj() {}
    
    /* Gets you a shared (const) claim. */
    AFK_SharedClaim<T> claimShared(unsigned int threadId, AFK_ClaimFlags flags)
    {
        bool claimed = false;

        do
        {
            if ((id.fetch_or(AFK_CL_THREAD_ID_SHARED(threadId)) & AFK_CL_NONSHARED) == AFK_CL_NONSHARED)
            {
                /* It's already claimed exclusively, flip that
                 * bit back
                 */
                id.fetch_and(AFK_CL_THREAD_ID_SHARED_MASK(threadId));

                if (flags & AFK_ClaimFlags::LOOP) boost::this_thread::yield();
            }
            else claimed = true;
        }
        while (!claimed && (flags & AFK_ClaimFlags::LOOP));

        if (!claimed) throw AFK_ClaimException();

        int64_t computingFrameNum = getComputingFrame().get();
        lastSeen.store(computingFrameNum);

        return AFK_SharedClaim<T>(threadId, this);
    }

    /* Gets you an unshared claim directly.
     * The `exclusive' flag causes the `lastSeenExclusively'
     * field to be incremented if it's not already equal to
     * the current frame; this mechanism locks out an object
     * from being claimed more than once per frame.
     */
    AFK_Claim<T> claim(unsigned int threadId, AFK_ClaimFlags flags)
    {
        bool claimed = false;

        do
        {
            uint64_t expected = AFK_CL_NO_THREAD;
            claimed = id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
            if (!claimed && (flags & AFK_ClaimFlags::LOOP)) boost::this_thread::yield();
        }
        while (!claimed && (flags & AFK_ClaimFlags::LOOP));

        if (!claimed) throw AFK_ClaimException();

        int64_t computingFrameNum = getComputingFrame().get();

        /* Help the evictor out a little */
        if (flags & AFK_ClaimFlags::EVICTOR)
        {
            if (lastSeen.load() == computingFrameNum)
            {
                release(threadId);
                throw AFK_ClaimException();
            }
        }
        else
        {
            lastSeen.store(computingFrameNum);
        
            if (flags & AFK_ClaimFlags::EXCLUSIVE)
            {
                if (lastSeenExclusively.exchange(computingFrameNum) == computingFrameNum)
                {
                    release(threadId);
                    throw AFK_ClaimException();
                }
            }
        }

        return AFK_Claim<T>(threadId, this);
    }

    template<typename T>
    friend class AFK_SharedClaim<T>;

    template<typename T>
    friend class AFK_Claim<T>;

    template<typename T, GetComputingFrame getComputingFrame>
    friend std::ostream& operator<< <T>(std::ostream& os, const AFK_Claimable<T, getComputingFrame>& c);
};

template<typename T, GetComputingFrame getComputingFrame>
std::ostream& operator<<(std::ostream& os, const AFK_Claimable<T, getComputingFrame>& c)
{
    /* I'm going to ignore any claims here and just read
     * out what's there -- it might be out of date, but
     * these functions are for diagnostics only and that
     * should be OK
     */
    os << "Claimable(" << c.obj.load() << ", last seen " << c.lastSeen.load() << ", last seen exclusively " << c.lastSeenExclusively.load() << ")";
    return os;
}

