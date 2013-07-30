/* AFK (c) Alex Holloway 2013 */

#include <sstream>

#include <boost/thread/thread.hpp>

#include "claimable.hpp"



AFK_Claimable::AFK_Claimable()
#if CLAIMABLE_MUTEX
#else
    :claimingThreadId(UNCLAIMED)
#endif
{
}

enum AFK_ClaimStatus AFK_Claimable::claim(unsigned int threadId, enum AFK_ClaimType type)
{
    AFK_Frame currentFrame = getCurrentFrame();

    AFK_ClaimStatus status = AFK_CL_TAKEN;
#if CLAIMABLE_MUTEX
    bool gotUpgradeLock = false;
    if (type == AFK_CLT_NONEXCLUSIVE_SHARED)
    {
        /* Try to give you an upgradable context. */
        if (claimingMut.try_lock_upgrade()) gotUpgradeLock = true;
        else claimingMut.lock_shared();
    }
    else claimingMut.lock();
#else
    unsigned int expectedId = UNCLAIMED;
    if (claimingThreadId.compare_exchange_strong(expectedId, threadId))
#endif
    {
        switch (type)
        {
        case AFK_CLT_EXCLUSIVE:
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
            /* You've definitely got it */
            lastSeen = currentFrame;
            status = AFK_CL_CLAIMED;
            break;

        case AFK_CLT_NONEXCLUSIVE_SHARED:
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
#if CLAIMABLE_MUTEX
#else
    else
    {
        /* I might as well check whether I'd ever give it to
         * you at all
         */
        if (type == AFK_CLT_EXCLUSIVE && lastSeenExclusively == currentFrame)
            status = AFK_CL_ALREADY_PROCESSED;
    }
#endif

    return status;
}

enum AFK_ClaimStatus AFK_Claimable::upgrade(unsigned int threadId, enum AFK_ClaimStatus status)
{
#if CLAIMABLE_MUTEX
    switch (status)
    {
    case AFK_CL_CLAIMED_UPGRADABLE:
        claimingMut.unlock_upgrade_and_lock();
        status = AFK_CL_CLAIMED;
        break;

    default:
        throw AFK_ClaimException();
    }
#else
    /* Not supported right now */
    throw AFK_ClaimException();
#endif

    return status;
}

void AFK_Claimable::release(unsigned int threadId, enum AFK_ClaimStatus status)
{
#if CLAIMABLE_MUTEX
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
#else
    if (!claimingThreadId.compare_exchange_strong(threadId, UNCLAIMED))
        throw AFK_ClaimException();
#endif
}

enum AFK_ClaimStatus AFK_Claimable::claimYieldLoop(unsigned int threadId, enum AFK_ClaimType type)
{
    enum AFK_ClaimStatus status = AFK_CL_TAKEN;
    for (unsigned int tries = 0; status == AFK_CL_TAKEN /* && tries < 2 */; ++tries)
    {
        status = claim(threadId, type);
        if (status == AFK_CL_TAKEN) boost::this_thread::yield();
    }

    return status;
}

