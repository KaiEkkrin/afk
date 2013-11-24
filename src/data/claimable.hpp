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
#include <thread>

#include <boost/atomic.hpp>

#include "frame.hpp"
#include "volatile.hpp"

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

template<typename T>
class AFK_Claim;

template<typename T>
class AFK_Claimable;

template<typename T>
class AFK_InplaceClaim
{
protected:
    unsigned int threadId; /* TODO change all thread IDs to 64-bit */
    AFK_Claimable<T> *claimable;
    bool shared;
    bool released;

    /* I need to be able to make a "blank", for the benefit of
     * Claim below.
     */
    AFK_InplaceClaim() noexcept: released(true) {}

    AFK_InplaceClaim(unsigned int _threadId, AFK_Claimable<T> *_claimable, bool _shared) noexcept:
        threadId(_threadId), claimable(_claimable), shared(_shared), released(false)
    {
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
    }

public:
    /* No reference counting is performed, so I must never copy a
     * claim around
     */
    AFK_InplaceClaim(const AFK_InplaceClaim& _c) = delete;
    AFK_InplaceClaim& operator=(const AFK_InplaceClaim& _c) = delete;

    /* I need a move constructor in order to return a claim from
     * the claimable's claim method.  (Although for whatever
     * reason, this and the move assign don't seem to get called
     * when acquiring world cell and landscape tile claims.
     * Hmm.
     * ...I wonder how prone to creating bugs this might be...
     */ 
    AFK_InplaceClaim(AFK_InplaceClaim&& _claim) noexcept:
        threadId(_claim.threadId), claimable(_claim.claimable), shared(_claim.shared), released(_claim.released)
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving inplace claim for " << std::hex << claimable << ": " << obj)
        _claim.released = true;
    }

    /* ...likewise... */
    AFK_InplaceClaim& operator=(AFK_InplaceClaim&& _claim) noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("assign moving inplace claim for " << std::hex << claimable << ": " << obj)

        threadId        = _claim.threadId;
        claimable       = _claim.claimable;
        shared          = _claim.shared;
        released        = _claim.released;
        _claim.released = true;
        return *this;
    }

    virtual ~AFK_InplaceClaim() noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing inplace claim for " << std::hex << claimable << ": " << obj << "(released: " << released << ")")
        if (!released) release();
    }

    const volatile T& getShared(void) const noexcept
    {
        assert(!released);
        return claimable->obj;
    }

    volatile T& get(void) noexcept
    {
        assert(!shared);
        assert(!released);
        return claimable->obj;
    }

    bool upgrade(void) noexcept
    {
        assert(!released);
        if (!shared) return true;

        if (claimable->tryUpgradeShared(threadId))
        {
            shared = false;
            return true;
        }
        
        return false;
    }

    void release(void) noexcept
    {
        assert(!released);
        if (shared)
        {
            claimable->releaseShared(threadId);
        }
        else
        {
            boost::atomic_thread_fence(boost::memory_order_seq_cst);
            claimable->release(threadId);
        }

        released = true;
    }

    /* This function releases the claim without swapping the object
     * back, discarding any changes.
     */
    void invalidate(void) noexcept
    {
        assert(!released);
        if (shared) claimable->releaseShared(threadId);
        else claimable->release(threadId);
        released = true;
    }

    friend class AFK_Claim<T>;
    friend class AFK_Claimable<T>;
};

template<typename T>
class AFK_Claim
{
protected:
    AFK_InplaceClaim<T> inplace;
    T obj;

    AFK_Claim(unsigned int _threadId, AFK_Claimable<T> *_claimable, bool _shared) noexcept:
        inplace(_threadId, _claimable, _shared)
    {
        afk_grabShared<T>(&obj, inplace.claimable->objPtr());
    }

public:
    /* I can make a Claim out of an inplace one (which invalidates
     * the inplace one).
     */
    AFK_Claim(AFK_InplaceClaim<T>& _inplace) noexcept
    {
        inplace.threadId    = _inplace.threadId;
        inplace.claimable   = _inplace.claimable;
        inplace.shared      = _inplace.shared;
        inplace.released    = _inplace.released;

        if (!_inplace.released)
        {
            afk_grabShared<T>(&obj, inplace.claimable->objPtr());

            /* Flag the old inplace claim as released.  It will be
             * unusable now!
             */
            _inplace.released = true;        
        }
    }

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
    AFK_Claim(AFK_Claim&& _claim) noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving claim for " << std::hex << claimable << ": " << obj)

        inplace.threadId    = _claim.inplace.threadId;
        inplace.claimable   = _claim.inplace.claimable;
        inplace.shared      = _claim.inplace.shared;
        inplace.released    = _claim.inplace.released;
        if (!inplace.released) obj = _claim.obj;
        _claim.inplace.released = true;
    }

    /* ...likewise... */
    AFK_Claim& operator=(AFK_Claim&& _claim) noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("assign moving claim for " << std::hex << claimable << ": " << obj)

        inplace.threadId    = _claim.inplace.threadId;
        inplace.claimable   = _claim.inplace.claimable;
        inplace.shared      = _claim.inplace.shared;
        inplace.released    = _claim.inplace.released;
        if (!inplace.released) obj = _claim.obj;
        _claim.inplace.released = true;
        return *this;
    }

    virtual ~AFK_Claim() noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing claim for " << std::hex << claimable << ": " << obj << "(released: " << released << ")")
        if (!inplace.released) release();
    }

    const T& getShared(void) const noexcept
    {
        assert(!inplace.released);
        return obj;
    }

    T& get(void) noexcept
    {
        assert(!inplace.shared);
        assert(!inplace.released);
        return obj;
    }

    bool upgrade(void) noexcept
    {
        return inplace.upgrade();
    }

    void release(void) noexcept
    {
        if (!inplace.released && !inplace.shared)
        {
            afk_returnShared<T>(&obj, inplace.claimable->objPtr());
        }

        inplace.release();
    }

    /* This function releases the claim without swapping the object
     * back, discarding any changes.
     */
    void invalidate(void) noexcept
    {
        inplace.invalidate();
    }

    friend class AFK_Claimable<T>;
};

#define AFK_CL_LOOP         1
#define AFK_CL_SPIN         2
#define AFK_CL_EXCLUSIVE    4
#define AFK_CL_EVICTOR      8
#define AFK_CL_SHARED      16

template<typename T>
class AFK_Claimable
{
protected:
    /* Which thread ID has claimed use of the object.
     * The thread ID is incremented by 1 to make sure no 0s are
     * knocking about; 0 means unclaimed, and the top bit is the
     * non-shared flag.
     */
    boost::atomic_uint_fast64_t id;

    /* The claimable object itself. */
    volatile T obj;

    /* Returns a pointer to it, so the Claim can copy its contents out and back in again */
    volatile T* objPtr(void) { return &obj; }

#define AFK_CL_NO_THREAD 0
#define AFK_CL_NONSHARED (1uLL<<63)

#define AFK_CL_THREAD_ID_SHARED(id) (1uLL<<((uint64_t)(id)))
#define AFK_CL_THREAD_ID_SHARED_MASK(id) (~AFK_CL_THREAD_ID_SHARED(id))

#define AFK_CL_THREAD_ID_NONSHARED(id) (AFK_CL_THREAD_ID_SHARED(id) | AFK_CL_NONSHARED)
#define AFK_CL_THREAD_ID_NONSHARED_MASK(id) (~AFK_CL_THREAD_ID_NONSHARED(id))

    bool tryClaim(unsigned int threadId) noexcept
    {
        uint64_t expected = AFK_CL_NO_THREAD;
        return id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
    }

    bool tryClaimShared(unsigned int threadId) noexcept
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

    bool tryUpgradeShared(unsigned int threadId) noexcept
    {
        uint64_t expected = AFK_CL_THREAD_ID_SHARED(threadId);
        return id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
    }

    void releaseShared(unsigned int threadId) noexcept
    {
        uint64_t old = id.fetch_and(AFK_CL_THREAD_ID_SHARED_MASK(threadId));
        assert(!(old & AFK_CL_NONSHARED));
    }

public:
    void release(unsigned int threadId) noexcept
    {
        id.fetch_and(AFK_CL_THREAD_ID_NONSHARED_MASK(threadId));
    }

    AFK_Claimable() noexcept: id(AFK_CL_NO_THREAD), obj()
    {
        assert(id.is_lock_free());
    }

    /* The move constructors are used to enable initialisation.
     * They essentially make a new Claimable.
     */
    AFK_Claimable(const AFK_Claimable&& _claimable) noexcept:
        id(AFK_CL_NO_THREAD)
    {
        obj = _claimable.obj;
    }

    AFK_Claimable& operator=(const AFK_Claimable&& _claimable) noexcept
    {
        id.store(AFK_CL_NO_THREAD);
        obj = _claimable.obj;
        return *this;
    }

    /* Claims as requested, but doesn't return any AFK_Claim object.
     * This is for the benefit of WatchedClaimable which can check for
     * watches and then return.
     * Instead this function returns true if successful, else false
     * If you are not WatchedClaimable DO NOT CALL THIS FUNCTION
     */
    bool claimInternal(unsigned int threadId, unsigned int flags) noexcept
    {
        bool claimed = false;

        do
        {
            if (flags & AFK_CL_SHARED) claimed = tryClaimShared(threadId);
            else claimed = tryClaim(threadId);
            if (flags & AFK_CL_LOOP) std::this_thread::yield();
        }
        while (!claimed && ((flags & AFK_CL_LOOP) || (flags & AFK_CL_SPIN)));

        return claimed;
    }

    /* Returns a claim object, assuming you claimed with claimInternal().
     * Again, for the benefit of WatchedClaimable.
     * If you are not WatchedClaimable DO NOT CALL THIS FUNCTION
     */
    AFK_Claim<T> getClaimable(unsigned int threadId, unsigned int flags) noexcept
    {
        return AFK_Claim<T>(threadId, this, flags & AFK_CL_SHARED);
    }

    /* As above, but returns an inplace claim. */
    AFK_InplaceClaim<T> getInplaceClaimable(unsigned int threadId, unsigned int flags) noexcept
    {
        return AFK_InplaceClaim<T>(threadId, this, flags & AFK_CL_SHARED);
    }

    /* Gets you a claim of the desired type.
     * The `exclusive' flag causes the `lastSeenExclusively'
     * field to be incremented if it's not already equal to
     * the current frame; this mechanism locks out an object
     * from being claimed more than once per frame.
     */
    AFK_Claim<T> claim(unsigned int threadId, unsigned int flags) 
    {
        bool claimed = claimInternal(threadId, flags);
        if (!claimed) throw AFK_ClaimException();
        return getClaimable(threadId, flags);
    }

    /* As above, but gets an inplace claim. */
    AFK_InplaceClaim<T> claimInplace(unsigned int threadId, unsigned int flags)
    {
        bool claimed = claimInternal(threadId, flags);
        if (!claimed) throw AFK_ClaimException();
        return getInplaceClaimable(threadId, flags);
    }

    friend class AFK_InplaceClaim<T>;
    friend class AFK_Claim<T>;

    template<typename _T>
    friend std::ostream& operator<<(std::ostream& os, const AFK_Claimable<_T>& c);
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const AFK_Claimable<T>& c)
{
    os << "Claimable(at " << std::hex << c.objPtr() << ")";
    return os;
}

/* How to specify a function to obtain the current computing frame */
typedef std::function<const AFK_Frame& (void)> AFK_GetComputingFrame;

template<typename T, AFK_GetComputingFrame& getComputingFrame>
class AFK_WatchedClaimable
{
protected:
    AFK_Claimable<T> claimable;

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
            assert(!(flags & AFK_CL_SHARED));
            if (lastSeen.load() == computingFrameNum) return false;
        }
        else
        {
            lastSeen.store(computingFrameNum);
        
            if (flags & AFK_CL_EXCLUSIVE)
            {
                assert(!(flags & AFK_CL_SHARED));
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

    AFK_Claim<T> claim(unsigned int threadId, unsigned int flags)
    {
        bool claimed = claimable.claimInternal(threadId, flags);
        if (!claimed) throw AFK_ClaimException();
        if (!watch(flags))
        {
            claimable.release(threadId);
            throw AFK_ClaimException();
        }

        return claimable.getClaimable(threadId, flags);
    }

    AFK_InplaceClaim<T> claimInplace(unsigned int threadId, unsigned int flags)
    {
        bool claimed = claimable.claimInternal(threadId, flags);
        if (!claimed) throw AFK_ClaimException();
        if (!watch(flags))
        {
            claimable.release(threadId);
            throw AFK_ClaimException();
        }

        return claimable.getInplaceClaimable(threadId, flags);
    }

    int64_t getLastSeen(void) const { return lastSeen.load(); }
    int64_t getLastSeenExclusively(void) const { return lastSeenExclusively.load(); }

    template<typename _T, AFK_GetComputingFrame& _getComputingFrame>
    friend std::ostream& operator<<(std::ostream& os, const AFK_WatchedClaimable<_T, _getComputingFrame>& c);
};

template<typename T, AFK_GetComputingFrame& getComputingFrame>
std::ostream& operator<<(std::ostream& os, const AFK_WatchedClaimable<T, getComputingFrame>& c)
{
    os << "WatchedClaimable(with " << c.claimable << ", last seen " << c.getLastSeen() << ", last seen exclusively " << c.getLastSeenExclusively() << ")";
    return os;
}

#endif /* _AFK_DATA_CLAIMABLE_H_ */

