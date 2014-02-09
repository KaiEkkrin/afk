/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#ifndef _AFK_DATA_CLAIMABLE_VOLATILE_H_
#define _AFK_DATA_CLAIMABLE_VOLATILE_H_

#include <cassert>
#include <sstream>
#include <thread>

#include <boost/atomic.hpp>

#include "claimable.hpp"
#include "data.hpp"
#include "volatile.hpp"

/* This defines the Claimable interface, using atomics and
 * volatile areas.
 */

template<typename T>
class AFK_VolatileClaim;

template<typename T>
class AFK_VolatileClaimable;

template<typename T>
class AFK_VolatileInplaceClaim
{
protected:
    unsigned int threadId; /* TODO change all thread IDs to 64-bit */
    AFK_VolatileClaimable<T> *claimable;
    bool shared;
    bool released;

    AFK_VolatileInplaceClaim(unsigned int _threadId, AFK_VolatileClaimable<T> *_claimable, bool _shared) afk_noexcept:
        threadId(_threadId), claimable(_claimable), shared(_shared), released(false)
    {
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
    }

public:
    /* I need to be able to make a "blank", for the benefit of
     * Claim below, and also to represent a claim failure.
     */
    AFK_VolatileInplaceClaim() afk_noexcept: released(true) {}

    /* No reference counting is performed, so I must never copy a
     * claim around
     */
    AFK_VolatileInplaceClaim(const AFK_VolatileInplaceClaim& _c) = delete;
    AFK_VolatileInplaceClaim& operator=(const AFK_VolatileInplaceClaim& _c) = delete;

    /* I need a move constructor in order to return a claim from
     * the claimable's claim method.  (Although for whatever
     * reason, this and the move assign don't seem to get called
     * when acquiring world cell and landscape tile claims.
     * Hmm.
     * ...I wonder how prone to creating bugs this might be...
     */ 
    AFK_VolatileInplaceClaim(AFK_VolatileInplaceClaim&& _claim) afk_noexcept:
        threadId(_claim.threadId), claimable(_claim.claimable), shared(_claim.shared), released(_claim.released)
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving inplace claim for " << std::hex << claimable << ": " << obj);
        _claim.released = true;
    }

    /* ...likewise... */
    AFK_VolatileInplaceClaim& operator=(AFK_VolatileInplaceClaim&& _claim) afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("assign moving inplace claim for " << std::hex << claimable << ": " << obj);

        threadId        = _claim.threadId;
        claimable       = _claim.claimable;
        shared          = _claim.shared;
        released        = _claim.released;
        _claim.released = true;
        return *this;
    }

    virtual ~AFK_VolatileInplaceClaim() afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing inplace claim for " << std::hex << claimable << ": " << obj << "(released: " << released << ")");
        if (!released) release();
    }

    bool isValid(void) const afk_noexcept
    {
        return !released;
    }

    const volatile T& getShared(void) const afk_noexcept
    {
        assert(!released);
        return claimable->obj;
    }

    volatile T& get(void) afk_noexcept
    {
        assert(!shared && !released);
        return claimable->obj;
    }

    bool upgrade(void) afk_noexcept
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

    void release(void) afk_noexcept
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
    void invalidate(void) afk_noexcept
    {
        assert(!released);
        if (shared) claimable->releaseShared(threadId);
        else claimable->release(threadId);
        released = true;
    }

    friend class AFK_VolatileClaim<T>;
    friend class AFK_VolatileClaimable<T>;
};

template<typename T>
class AFK_VolatileClaim
{
protected:
    AFK_VolatileInplaceClaim<T> inplace;
    T obj;

    AFK_VolatileClaim(unsigned int _threadId, AFK_VolatileClaimable<T> *_claimable, bool _shared) afk_noexcept:
        inplace(_threadId, _claimable, _shared)
    {
        afk_grabShared<T>(&obj, inplace.claimable->objPtr());
    }

public:
    AFK_VolatileClaim() afk_noexcept: inplace() {}

    /* I can make a Claim out of an inplace one (which invalidates
     * the inplace one).
     */
    AFK_VolatileClaim(AFK_VolatileInplaceClaim<T>& _inplace) afk_noexcept
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
    AFK_VolatileClaim(const AFK_VolatileClaim& _c) = delete;
    AFK_VolatileClaim& operator=(const AFK_VolatileClaim& _c) = delete;

    /* I need a move constructor in order to return a claim from
     * the claimable's claim method.  (Although for whatever
     * reason, this and the move assign don't seem to get called
     * when acquiring world cell and landscape tile claims.
     * Hmm.
     * ...I wonder how prone to creating bugs this might be...
     */ 
    AFK_VolatileClaim(AFK_VolatileClaim&& _claim) afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving claim for " << std::hex << claimable << ": " << obj);

        inplace.threadId    = _claim.inplace.threadId;
        inplace.claimable   = _claim.inplace.claimable;
        inplace.shared      = _claim.inplace.shared;
        inplace.released    = _claim.inplace.released;
        if (!inplace.released) obj = _claim.obj;
        _claim.inplace.released = true;
    }

    /* ...likewise... */
    AFK_VolatileClaim& operator=(AFK_VolatileClaim&& _claim) afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("assign moving claim for " << std::hex << claimable << ": " << obj);

        inplace.threadId    = _claim.inplace.threadId;
        inplace.claimable   = _claim.inplace.claimable;
        inplace.shared      = _claim.inplace.shared;
        inplace.released    = _claim.inplace.released;
        if (!inplace.released) obj = _claim.obj;
        _claim.inplace.released = true;
        return *this;
    }

    virtual ~AFK_VolatileClaim() afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing claim for " << std::hex << claimable << ": " << obj << "(released: " << released << ")");
        if (!inplace.released) release();
    }

    bool isValid(void) const afk_noexcept
    {
        return !inplace.released;
    }

    const T& getShared(void) const afk_noexcept
    {
        assert(!inplace.released);
        return obj;
    }

    T& get(void) afk_noexcept
    {
        assert(!inplace.shared);
        assert(!inplace.released);
        return obj;
    }

    bool upgrade(void) afk_noexcept
    {
        return inplace.upgrade();
    }

    void release(void) afk_noexcept
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
    void invalidate(void) afk_noexcept
    {
        inplace.invalidate();
    }

    friend class AFK_VolatileClaimable<T>;
};

template<typename T>
class AFK_VolatileClaimable
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

    bool tryClaim(unsigned int threadId) afk_noexcept
    {
        uint64_t expected = AFK_CL_NO_THREAD;
        return id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
    }

    bool tryClaimShared(unsigned int threadId) afk_noexcept
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

    bool tryUpgradeShared(unsigned int threadId) afk_noexcept
    {
        uint64_t expected = AFK_CL_THREAD_ID_SHARED(threadId);
        return id.compare_exchange_strong(expected, AFK_CL_THREAD_ID_NONSHARED(threadId));
    }

    void releaseShared(unsigned int threadId) afk_noexcept
    {
#ifdef NDEBUG
        id.fetch_and(AFK_CL_THREAD_ID_SHARED_MASK(threadId));
#else
        uint64_t old = id.fetch_and(AFK_CL_THREAD_ID_SHARED_MASK(threadId));
        assert(!(old & AFK_CL_NONSHARED));
#endif
    }

public:
    void release(unsigned int threadId) afk_noexcept
    {
        id.fetch_and(AFK_CL_THREAD_ID_NONSHARED_MASK(threadId));
    }

    AFK_VolatileClaimable() afk_noexcept: id(AFK_CL_NO_THREAD), obj()
    {
        assert(id.is_lock_free());
    }

    /* The move constructors are used to enable initialisation.
     * They essentially make a new Claimable.
     */
    AFK_VolatileClaimable(const AFK_VolatileClaimable&& _claimable) afk_noexcept:
        id(AFK_CL_NO_THREAD)
    {
        obj = _claimable.obj;
    }

    AFK_VolatileClaimable& operator=(const AFK_VolatileClaimable&& _claimable) afk_noexcept
    {
        id.store(AFK_CL_NO_THREAD);
        obj = _claimable.obj;
        return *this;
    }

    /* Claims as requested, but doesn't return any AFK_VolatileClaim object.
     * This is for the benefit of WatchedClaimable which can check for
     * watches and then return.
     * Instead this function returns true if successful, else false
     * If you are not WatchedClaimable DO NOT CALL THIS FUNCTION
     */
    bool claimInternal(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        bool claimed = false;

        do
        {
            if (AFK_CL_IS_SHARED(flags)) claimed = tryClaimShared(threadId);
            else claimed = tryClaim(threadId);
            if (!claimed && (flags & AFK_CL_LOOP)) std::this_thread::yield();
        }
        while (!claimed && ((flags & AFK_CL_LOOP) || (flags & AFK_CL_SPIN)));

        return claimed;
    }

    /* Returns a claim object, assuming you claimed with claimInternal().
     * Again, for the benefit of WatchedClaimable.
     * If you are not WatchedClaimable DO NOT CALL THIS FUNCTION
     */
    AFK_VolatileClaim<T> getClaim(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        return AFK_VolatileClaim<T>(threadId, this, AFK_CL_IS_SHARED(flags));
    }

    /* As above, but returns an inplace claim. */
    AFK_VolatileInplaceClaim<T> getInplaceClaim(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        return AFK_VolatileInplaceClaim<T>(threadId, this, AFK_CL_IS_SHARED(flags));
    }

    /* Gets you a claim of the desired type.  If it fails,
     * an released claim is returned.
     * The `exclusive' flag causes the `lastSeenExclusively'
     * field to be incremented if it's not already equal to
     * the current frame; this mechanism locks out an object
     * from being claimed more than once per frame.
     */
    AFK_VolatileClaim<T> claim(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        bool claimed = claimInternal(threadId, flags);
        if (claimed) return getClaim(threadId, flags);
        else return AFK_VolatileClaim<T>();
    }

    /* As above, but gets an inplace claim. */
    AFK_VolatileInplaceClaim<T> claimInplace(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        bool claimed = claimInternal(threadId, flags);
        if (claimed) return getInplaceClaim(threadId, flags);
        else return AFK_VolatileInplaceClaim<T>();
    }

    friend class AFK_VolatileInplaceClaim<T>;
    friend class AFK_VolatileClaim<T>;

    template<typename _T>
    friend std::ostream& operator<<(std::ostream& os, const AFK_VolatileClaimable<_T>& c);
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const AFK_VolatileClaimable<T>& c)
{
    os << "VolatileClaimable(at " << std::hex << c.objPtr() << ")";
    return os;
}

#endif /* _AFK_DATA_CLAIMABLE_VOLATILE_H_ */

