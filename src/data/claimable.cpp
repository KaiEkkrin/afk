/* AFK (c) Alex Holloway 2013 */

#include <sstream>

#include <boost/thread/thread.hpp>

#include "claimable.hpp"



AFK_Claimable::AFK_Claimable():
    claimingThreadId(UNCLAIMED)
{
}

enum AFK_ClaimStatus AFK_Claimable::claim(unsigned int threadId, enum AFK_ClaimType type)
{
    AFK_Frame currentFrame = getCurrentFrame();
    unsigned int expectedId = UNCLAIMED;
    AFK_ClaimStatus status = AFK_CL_TAKEN;
    if (claimingThreadId.compare_exchange_strong(expectedId, threadId))
    {
        switch (type)
        {
        case AFK_CLT_EXCLUSIVE:
            if (lastSeenExclusively == currentFrame)
            {
                /* This cell already got processed this frame,
                 * it shouldn't get claimed again
                 */
                release(threadId);
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

        case AFK_CLT_EVICTOR:
            /* You've definitely got it, and I shouldn't bump
             * lastSeen
             */
            status = AFK_CL_CLAIMED;
            break;

        default:
            /* This is a programming error */
            throw new AFK_ClaimException();
        }
    }
    else
    {
        /* I might as well check whether I'd ever give it to
         * you at all
         */
        if (type == AFK_CLT_EXCLUSIVE && lastSeenExclusively == currentFrame)
            status = AFK_CL_ALREADY_PROCESSED;
    }

    return status;
}

void AFK_Claimable::release(unsigned int threadId)
{
    if (!claimingThreadId.compare_exchange_strong(threadId, UNCLAIMED))
        throw AFK_ClaimException();
}

bool AFK_Claimable::claimYieldLoop(unsigned int threadId, enum AFK_ClaimType type)
{
    bool claimed = false;
    for (unsigned int tries = 0; !claimed && tries < 2; ++tries)
    {
        enum AFK_ClaimStatus status = claim(threadId, type);
        switch (status)
        {
        case AFK_CL_CLAIMED:
            claimed = true;
            break;

        case AFK_CL_ALREADY_PROCESSED:
            return false;

        case AFK_CL_TAKEN:
            boost::this_thread::yield();
            break;

        default:
            throw new AFK_ClaimException();
        }
    }

    return claimed;
}

