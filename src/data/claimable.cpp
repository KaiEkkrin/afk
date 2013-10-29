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

#include <sstream>

#include <boost/thread/thread.hpp>

#include "claimable.hpp"


#if CLAIMABLE_MUTEX

AFK_Claimable::AFK_Claimable()
{
}

enum AFK_ClaimStatus AFK_Claimable::claim(unsigned int threadId, enum AFK_ClaimType type, const AFK_Frame& currentFrame)
{
    AFK_ClaimStatus status = AFK_CL_TAKEN;
    bool gotUpgradeLock = false;
    switch (type)
    {
    case AFK_CLT_NONEXCLUSIVE_SHARED:
        /* Try to give you an upgradable context. */
        if (claimingMut.try_lock_upgrade()) gotUpgradeLock = true;
        else claimingMut.lock_shared();
        break;

    case AFK_CLT_NONEXCLUSIVE_UPGRADE:
        /* Always give you an upgradable context. */
        claimingMut.lock_upgrade();
        gotUpgradeLock = true;
        break;

    default:
        claimingMut.lock();
        break;
    }
    {
        switch (type)
        {
        case AFK_CLT_EXCLUSIVE:
            if (currentFrame.getNever()) throw AFK_ClaimException();

            if (lastSeenExclusively == currentFrame)
            {
                /* This cell already got processed this frame,
                 * it shouldn't get claimed again
                 */
                release(threadId, AFK_CL_CLAIMED);
                status = AFK_CL_ALREADY_PROCESSED;
            }
            else
            {
                /* You've got it */
                lastSeen = currentFrame;
                lastSeenExclusively = currentFrame;
                status = AFK_CL_CLAIMED;
            }
            break;

        case AFK_CLT_NONEXCLUSIVE:
            if (currentFrame.getNever()) throw AFK_ClaimException();

            /* You've definitely got it */
            lastSeen = currentFrame;
            status = AFK_CL_CLAIMED;
            break;

        case AFK_CLT_NONEXCLUSIVE_SHARED:
        case AFK_CLT_NONEXCLUSIVE_UPGRADE:
            if (currentFrame.getNever()) throw AFK_ClaimException();

            /* You've also definitely got it, but in what way? */
            lastSeen = currentFrame;
            status = gotUpgradeLock ? AFK_CL_CLAIMED_UPGRADABLE : AFK_CL_CLAIMED_SHARED;
            break;

        case AFK_CLT_EVICTOR:
            /* You've definitely got it, and I shouldn't bump
             * lastSeen
             */
            status = AFK_CL_CLAIMED;
            break;

        default:
            /* This is a programming error */
            throw AFK_ClaimException();
        }
    }

    return status;
}

enum AFK_ClaimStatus AFK_Claimable::upgrade(unsigned int threadId, enum AFK_ClaimStatus status)
{
    switch (status)
    {
    case AFK_CL_CLAIMED_UPGRADABLE:
        claimingMut.unlock_upgrade_and_lock();
        status = AFK_CL_CLAIMED;
        break;

    default:
        throw AFK_ClaimException();
    }

    return status;
}

void AFK_Claimable::release(unsigned int threadId, enum AFK_ClaimStatus status)
{
    switch (status)
    {
    case AFK_CL_CLAIMED:
        claimingMut.unlock();
        break;

    case AFK_CL_CLAIMED_UPGRADABLE:
        claimingMut.unlock_upgrade();
        break;

    case AFK_CL_CLAIMED_SHARED:
        claimingMut.unlock_shared();
        break;

    default:
        /* Another programming error */
        throw AFK_ClaimException();
    }
}

#else /* CLAIMABLE_MUTEX */

#define NO_THREAD 0
#define NONSHARED (1uLL<<63)

#define THREAD_ID_SHARED(id) (((uint64_t)(id))+1uLL)
#define THREAD_ID_SHARED_MASK(id) (~THREAD_ID_SHARED(id))

#define THREAD_ID_NONSHARED(id) (THREAD_ID_SHARED(id) | NONSHARED)
#define THREAD_ID_NONSHARED_MASK(id) (~THREAD_ID_NONSHARED(id))

AFK_Claimable::AFK_Claimable():
    claimingThreadId(NO_THREAD)
{
}

enum AFK_ClaimStatus AFK_Claimable::claim(unsigned int threadId, enum AFK_ClaimType type, const AFK_Frame& currentFrame)
{
    AFK_ClaimStatus status = AFK_CL_TAKEN;

    switch (type)
    {
    case AFK_CLT_NONEXCLUSIVE_SHARED:
    case AFK_CLT_NONEXCLUSIVE_UPGRADE:
        /* These are the same in non-mutex land.  Try to take it
         * without having a set nonshared bit.
         */
        if ((claimingThreadId.fetch_or(THREAD_ID_SHARED(threadId)) & NONSHARED) == NONSHARED)
        {
            /* Someone's got it exclusively!  Flip that bit back. */
            claimingThreadId.fetch_and(THREAD_ID_SHARED_MASK(threadId));
        }
        else
        {
            status = AFK_CL_CLAIMED_UPGRADABLE;
        }
        break;

    case AFK_CLT_EXCLUSIVE:
    case AFK_CLT_NONEXCLUSIVE:
    case AFK_CLT_EVICTOR:
        /* Likewise.  Try to take it and set the nonshared bit.
         * For this to work you shouldn't have it at all
         */
        uint64_t expected = NO_THREAD;
        if (claimingThreadId.compare_exchange_strong(expected, THREAD_ID_NONSHARED(threadId)))
        {
            status = AFK_CL_CLAIMED;
        }
        break;
    }

    if (status == AFK_CL_CLAIMED)
    {
        /* You got a claim; sort out the frame tracking. */
        switch (type)
        {
        case AFK_CLT_EXCLUSIVE:
            if (lastSeenExclusively == currentFrame)
            {
                /* You've already processed this. */
                release(threadId, AFK_CL_CLAIMED);
                status = AFK_CL_ALREADY_PROCESSED;
            }
            else
            {
                /* Bump both last seen fields. */
                lastSeen = currentFrame;
                lastSeenExclusively = currentFrame;
            }
            break;

        case AFK_CLT_EVICTOR:
            /* Don't bother bumping anything */
            break;

        default:
            /* Bump only lastSeen. */
            lastSeen = currentFrame;
            break;
        }
    }

    return status;
}

enum AFK_ClaimStatus AFK_Claimable::upgrade(unsigned int threadId, enum AFK_ClaimStatus status)
{
    assert(status == AFK_CL_CLAIMED_UPGRADABLE);
    uint64_t expected = THREAD_ID_SHARED(threadId);
    if (claimingThreadId.compare_exchange_strong(expected, THREAD_ID_NONSHARED(threadId)))
    {
        status = AFK_CL_CLAIMED;
    }

    return status;
}

void AFK_Claimable::release(unsigned int threadId, enum AFK_ClaimStatus status)
{
    /* This oughtn't to make a difference, but just to be
     * sure, I shouldn't clear the nonshared bit with shared
     * statuses
     */
    switch (status)
    {
    case AFK_CL_CLAIMED:
        claimingThreadId.fetch_and(THREAD_ID_NONSHARED_MASK(threadId));
        break;

    case AFK_CL_CLAIMED_UPGRADABLE:
        claimingThreadId.fetch_and(THREAD_ID_SHARED_MASK(threadId));
        break;

    default:
        throw AFK_ClaimException();
    }
}

#endif /* CLAIMABLE_MUTEX */

enum AFK_ClaimStatus AFK_Claimable::claimYieldLoop(unsigned int threadId, enum AFK_ClaimType type, const AFK_Frame& currentFrame)
{
    enum AFK_ClaimStatus status = AFK_CL_TAKEN;
    while (status == AFK_CL_TAKEN)
    {
        status = claim(threadId, type, currentFrame);
        if (status == AFK_CL_TAKEN) boost::this_thread::yield();
    }

    return status;
}

