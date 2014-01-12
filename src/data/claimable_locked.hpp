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

#ifndef _AFK_DATA_CLAIMABLE_LOCKED_H_
#define _AFK_DATA_CLAIMABLE_LOCKED_H_

#include <boost/thread.hpp>

#include "claimable.hpp"
#include "data.hpp"

/* This defines the same interface as Claimable, but using boost
 * upgrade mutexes.
 * There's no separate "InplaceClaim" here, but it handles `shared'
 * and `upgrade' separately.
 * See claimable_volatile for explanatory comments.
 */

template<typename T>
class AFK_LockedClaimable;

enum class AFK_LockedClaimStatus : int
{
    Unique      = 0,
    Shared      = 1,
    Upgrade     = 2,
    Released    = 3
};


template<typename T>
class AFK_LockedClaim
{
protected:
    AFK_LockedClaimable<T> *claimable;
    AFK_LockedClaimStatus status;

    AFK_LockedClaim(AFK_LockedClaimable<T> *_claimable, bool _shared, bool _upgrade) afk_noexcept:
        claimable(_claimable)
    {
        assert(!(_shared && _upgrade));
        if (_shared) status = AFK_LockedClaimStatus::Shared;
        else if (_upgrade) status = AFK_LockedClaimStatus::Upgrade;
        else status = AFK_LockedClaimStatus::Unique;
    }

public:
    AFK_LockedClaim() afk_noexcept: claimable(nullptr), status(AFK_LockedClaimStatus::Released) {}

    AFK_LockedClaim(const AFK_LockedClaim& _c) = delete;
    AFK_LockedClaim& operator=(const AFK_LockedClaim& _c) = delete;

    /* Move construction and assignment, just like volatile claims */
    AFK_LockedClaim(AFK_LockedClaim&& _claim) afk_noexcept:
        claimable(_claim.claimable), status(_claim.status)
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("copy moving inplace claim for " << std::hex << claimable << ": " << obj)
        _claim.status   = AFK_LockedClaimStatus::Released;
    }

    AFK_LockedClaim& operator=(AFK_LockedClaim&& _claim) afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("assign moving inplace claim for " << std::hex << claimable << ": " << obj)

        claimable       = _claim.claimable;
        status          = _claim.status;
        _claim.status   = AFK_LockedClaimStatus::Released;
        return *this;
    }

    virtual ~AFK_LockedClaim() afk_noexcept
    {
        AFK_DEBUG_PRINTL_CLAIMABLE("destructing inplace claim for " << std::hex << claimable << ": " << obj << "(released: " << status == AFK_LockedClaimStatus::Released << ")")
        if (status != AFK_LockedClaimStatus::Released) release();
    }

    bool isValid(void) const afk_noexcept
    {
        return (status != AFK_LockedClaimStatus::Released);
    }

    const T& getShared(void) const afk_noexcept
    {
        assert(status != AFK_LockedClaimStatus::Released);
        return claimable->obj;
    }

    T& get(void) afk_noexcept
    {
        assert(status == AFK_LockedClaimStatus::Unique);
        return claimable->obj;
    }

    bool upgrade(void) afk_noexcept
    {
        switch (status)
        {
        case AFK_LockedClaimStatus::Unique:
            /* Already upgraded. */
            return true;

        case AFK_LockedClaimStatus::Upgrade:
            claimable->mut.unlock_upgrade_and_lock();
            status = AFK_LockedClaimStatus::Unique;
            return true;

        default:
            assert(false);
            return false;
        }
    }

    void release(void) afk_noexcept
    {
        switch (status)
        {
        case AFK_LockedClaimStatus::Unique:
            claimable->mut.unlock();
            break;

        case AFK_LockedClaimStatus::Shared:
            claimable->mut.unlock_shared();
            break;

        case AFK_LockedClaimStatus::Upgrade:
            claimable->mut.unlock_upgrade();
            break;

        default:
            assert(false); /* bug */
            break;
        }

        status = AFK_LockedClaimStatus::Released;
    }

    void invalidate(void) afk_noexcept
    {
        /* In this case, the values have been changed already, nothing
         * we can do...
         */
        release();
    }

    friend class AFK_LockedClaimable<T>;
};

template<typename T>
class AFK_LockedClaimable
{
protected:
    /* The claimable object. */
    T obj;

    /* TODO: Rather than having an inline mutex, try using a mutex
     * pool?  (saves creating some?)
     */
    boost::upgrade_mutex mut;

public:
    void release(unsigned int threadId) afk_noexcept
    {
        mut.unlock();
    }

    AFK_LockedClaimable() afk_noexcept: obj()
    {
        boost::unique_lock<boost::upgrade_mutex> lock(mut);
    }

    AFK_LockedClaimable(const AFK_LockedClaimable&& _claimable) afk_noexcept
    {
        boost::unique_lock<boost::upgrade_mutex> lock1(_claimable.mut);
        boost::unique_lock<boost::upgrade_mutex> lock2(mut);

        obj = _claimable.obj;
    }

    AFK_LockedClaimable& operator=(const AFK_LockedClaimable&& _claimable) afk_noexcept
    {
        boost::unique_lock<boost::upgrade_mutex> lock1(_claimable.mut);
        boost::unique_lock<boost::upgrade_mutex> lock2(mut);

        obj = _claimable.obj;
        return *this;
    }

    bool claimInternal(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        bool claimed = false;

        if (AFK_CL_IS_BLOCKING(flags))
        {
            if ((flags & AFK_CL_SHARED) != 0) mut.lock_shared();
            else if ((flags & AFK_CL_UPGRADE) != 0) mut.lock_upgrade();
            else mut.lock();

            claimed = true;
        }
        else
        {
            if ((flags & AFK_CL_SHARED) != 0) claimed = mut.try_lock_shared();
            else if ((flags & AFK_CL_UPGRADE) != 0) claimed = mut.try_lock_upgrade();
            else claimed = mut.try_lock();
        }

        return claimed;
    }

    AFK_LockedClaim<T> getClaim(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        return AFK_LockedClaim<T>(this, (flags & AFK_CL_SHARED) != 0, (flags & AFK_CL_UPGRADE) != 0);
    }

    AFK_LockedClaim<T> getInplaceClaim(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        return AFK_LockedClaim<T>(this, (flags & AFK_CL_SHARED) != 0, (flags & AFK_CL_UPGRADE) != 0);
    }

    AFK_LockedClaim<T> claim(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        bool claimed = claimInternal(threadId, flags);
        if (claimed) return getClaim(threadId, flags);
        else return AFK_LockedClaim<T>();
    }

    AFK_LockedClaim<T> claimInplace(unsigned int threadId, unsigned int flags) afk_noexcept
    {
        bool claimed = claimInternal(threadId, flags);
        if (claimed) return getInplaceClaim(threadId, flags);
        else return AFK_LockedClaim<T>();
    }

    friend class AFK_LockedClaim<T>;

    template<typename _T>
    friend std::ostream& operator<<(std::ostream& os, const AFK_LockedClaimable<_T>& c);
};

template<typename T>
std::ostream& operator<<(std::ostream& os, const AFK_LockedClaimable<T>& c)
{
    os << "LockedClaimable(at " << std::hex << c.objPtr() << ")";
    return os;
}

#endif /* _AFK_DATA_CLAIMABLE_LOCKED_H_ */

