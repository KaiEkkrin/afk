/* AFK (c) Alex Holloway 2013 */

#include <sstream>

#include <boost/thread/thread.hpp>

#include "claimable.hpp"



AFK_Claimable::AFK_Claimable():
    claimingThreadId(UNCLAIMED)
{
}

enum AFK_ClaimStatus AFK_Claimable::claim(unsigned int threadId, bool touch)
{
    AFK_Frame currentFrame = getCurrentFrame();
    unsigned int expectedId = UNCLAIMED;
    if (claimingThreadId.compare_exchange_strong(expectedId, threadId))
    {
        if (touch && lastSeen == currentFrame)
        {
            /* This cell already got processed this frame,
             * it shouldn't get claimed again
             */
            release(threadId);
            return AFK_CL_ALREADY_PROCESSED;
        }
        else
        {
            if (touch) lastSeen = currentFrame;
            return AFK_CL_CLAIMED;
        }
    }
    else
    {
        if (touch && lastSeen == currentFrame) return AFK_CL_ALREADY_PROCESSED;
        else return AFK_CL_TAKEN;
    }
}

void AFK_Claimable::release(unsigned int threadId)
{
    if (!claimingThreadId.compare_exchange_strong(threadId, UNCLAIMED))
        throw AFK_ClaimException();
}

bool AFK_Claimable::claimYieldLoop(unsigned int threadId, bool touch)
{
    bool claimed = false;
    for (unsigned int tries = 0; !claimed /* && tries < 2 */; ++tries)
    {
        enum AFK_ClaimStatus status = claim(threadId, touch);
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

