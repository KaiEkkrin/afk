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

/* These utilities transfer the volatile shared structures into
 * usable things and back again.
 */

template<typename T, typename I>
void afk_grabSharedIntegral(T *mine, const volatile T *shared, size_t& offset) noexcept
{
    *(reinterpret_cast<I*>(mine) + offset / sizeof(I)) =
        *(reinterpret_cast<const volatile I*>(shared) + offset / sizeof(I));
    offset += sizeof(I);
}

template<typename T>
void afk_grabShared(T *mine, const volatile T *shared) noexcept
{
    size_t offset = 0;
    while (offset < sizeof(T))
    {
        switch (sizeof(T) - offset)
        {
        case 1:
            afk_grabSharedIntegral<T, uint8_t>(mine, shared, offset);
            break;

        case 2: case 3:
            afk_grabSharedIntegral<T, uint16_t>(mine, shared, offset);
            break;

        case 4: case 5: case 6: case 7:
            afk_grabSharedIntegral<T, uint32_t>(mine, shared, offset);
            break;

        default:
            afk_grabSharedIntegral<T, uint64_t>(mine, shared, offset);
            break;
        }
    }
}

template<typename T, typename I>
void afk_returnSharedIntegral(const T *mine, volatile T *shared, size_t& offset) noexcept
{
    *(reinterpret_cast<volatile I*>(shared) + offset / sizeof(I)) =
        *(reinterpret_cast<const I*>(mine) + offset / sizeof(I));
    offset += sizeof(I);
}

template<typename T>
void afk_returnShared(const T *mine, volatile T *shared) noexcept
{
    size_t offset = 0;
    while (offset < sizeof(T))
    {
        switch (offset)
        {
        case 1:
            afk_returnSharedIntegral<T, uint8_t>(mine, shared, offset);
            break;

        case 2: case 3:
            afk_returnSharedIntegral<T, uint16_t>(mine, shared, offset);
            break;

        case 4: case 5: case 6: case 7:
            afk_returnSharedIntegral<T, uint32_t>(mine, shared, offset);
            break;

        default:
            afk_returnSharedIntegral<T, uint64_t>(mine, shared, offset);
            break;
        }
    }
}

template<typename T, typename I>
bool afk_equalsSharedIntegral(const volatile T *mine, const T *other, size_t& offset) noexcept
{
    bool equals = (*(reinterpret_cast<const volatile I*>(mine) + offset / sizeof(I)) ==
        *(reinterpret_cast<const I*>(other) + offset / sizeof(I)));
    offset += sizeof(I);
    return equals;
}

template<typename T>
bool afk_equalsShared(const volatile T *mine, const T *other) noexcept
{
    bool equals = true;
    size_t offset = 0;
    while (equals && offset < sizeof(T))
    {
        switch (sizeof(T) - offset)
        {
        case 1:
            equals &= afk_equalsSharedIntegral<T, uint8_t>(mine, other, offset);
            break;

        case 2: case 3:
            equals &= afk_equalsSharedIntegral<T, uint16_t>(mine, other, offset);
            break;

        case 4: case 5: case 6: case 7:
            equals &= afk_equalsSharedIntegral<T, uint32_t>(mine, other, offset);
            break;

        default:
            equals &= afk_equalsSharedIntegral<T, uint64_t>(mine, other, offset);
            break;
        }
    }

    return equals;   
}


template<typename T>
class AFK_Claimable;

template<typename T>
class AFK_Claim
{
protected:
    unsigned int threadId; /* TODO change all thread IDs to 64-bit */
    AFK_Claimable<T> *claimable;
    bool shared;
    bool released;

    T obj;

    AFK_Claim(unsigned int _threadId, AFK_Claimable<T> *_claimable, bool _shared) noexcept:
        threadId(_threadId), claimable(_claimable), shared(_shared), released(false)
    {
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
        afk_grabShared<T>(&obj, claimable->objPtr());
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
    AFK_Claim(AFK_Claim&& _claim) noexcept:
        threadId(_claim.threadId), claimable(_claim.claimable), shared(_claim.shared), released(_claim.released),
        obj(_claim.obj)
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving claim for " << std::hex << claimable << ": " << obj)
        _claim.released = true;
    }

    /* ...likewise... */
    AFK_Claim& operator=(AFK_Claim&& _claim) noexcept
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

    virtual ~AFK_Claim() noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing claim for " << std::hex << claimable << ": " << obj << "(released: " << released << ")")
        if (!released) release();
     }

    const T& getShared(void) const noexcept
    {
        assert(!released);
        return obj;
    }

    T& get(void) noexcept
    {
        assert(!shared);
        assert(!released);
        return obj;
    }

    bool upgrade(void) noexcept
    {
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
            afk_returnShared<T>(&obj, claimable->objPtr());
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
    boost::atomic<uint64_t> id;

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

    AFK_Claimable() noexcept: id(AFK_CL_NO_THREAD), obj() {}

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

    /* This is a shortcut that avoids making AFK_Claim objects.
     */
    bool match(unsigned int threadId, const T& other) noexcept
    {
        bool equals = false;

        bool claimed = claimInternal(threadId, AFK_CL_SPIN | AFK_CL_SHARED);
        assert(claimed);

        boost::atomic_thread_fence(boost::memory_order_seq_cst);
        equals = afk_equalsShared<T>(&obj, &other);
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
        releaseShared(threadId);

        return equals;
    }

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
    boost::atomic<int64_t> lastSeen;
    boost::atomic<int64_t> lastSeenExclusively;

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

        int64_t computingFrameNum = getComputingFrame().get();

        /* Help the evictor out a little */
        if (flags & AFK_CL_EVICTOR)
        {
            assert(!(flags & AFK_CL_SHARED));
            if (lastSeen.load() == computingFrameNum)
            {
                claimable.release(threadId);
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
                    claimable.release(threadId);
                    throw AFK_ClaimException();
                }
            }
        }

        return claimable.getClaimable(threadId, flags);
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

