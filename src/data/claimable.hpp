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

#include <exception>

#include <boost/atomic.hpp>

#include "frame.hpp"

/* A "Claimable" is a lockable thing with useful features. 
 * It can optionally
 * stamp the object with the frame time of use, and exclude
 * re-use until the next frame.
 */

/* Optionally, define this to try using a mutex instead.
 */
#define CLAIMABLE_MUTEX 0

#if CLAIMABLE_MUTEX
#include <boost/thread/shared_mutex.hpp>
#endif

/* Here are the different ways in which we can claim a cell. */
enum AFK_ClaimType
{
    AFK_CLT_EXCLUSIVE,          /* Wants to be the only claim to processing the cell this frame */
    AFK_CLT_NONEXCLUSIVE,       /* Bump the frame, but other threads may claim it this frame after
                                 * we've released.  Still unique and writable while holding the
                                 * claim.
                                 */
    AFK_CLT_NONEXCLUSIVE_SHARED,/* Bump the frame and hold a shared lock -- other threads may
                                 * claim it simultaneously, but no non-shared ones.  *Don't write*.
                                 * Not supported by the non-mutex version -- you get a non-shared
                                 * claim instead.
                                 * This may return AFK_CL_CLAIMED_UPGRADEABLE to you -- if so, you
                                 * can call upgrade() to get the object all to yourself for a while
                                 * (ironically, an AFK_CLT_NONEXCLUSIVE :) ) and write to it.
                                 * Otherwise, if you need to do something to it that requires
                                 * writing, you'd better release your claim and go do something else
                                 * for a while before retrying.
                                 */
    AFK_CLT_NONEXCLUSIVE_UPGRADE,/* This is like AFK_CLT_NONEXCLUSIVE_SHARED, but will always
                                  * return AFK_CL_CLAIMED_UPGRADEABLE (or AFK_CL_TAKEN).  It may
                                  * be slower.  For those occasions when you know you're going
                                  * to upgrade on a shared claimable
                                  */
    AFK_CLT_EVICTOR             /* We're the evictor.  Don't bump the frame. */
};

/* First here are the possible results from a claim attempt.
 */
enum AFK_ClaimStatus
{
    AFK_CL_CLAIMED,            /* You've got it */
    AFK_CL_CLAIMED_UPGRADABLE, /* You've got it, in an upgradable context */
    AFK_CL_CLAIMED_SHARED,     /* You've got it, in a shared context */
    AFK_CL_ALREADY_PROCESSED,  /* It's already been processed this frame, and you wanted an
                                * exclusive claim
                                */
    AFK_CL_TAKEN               /* Someone else has it */
};

/* For errors.  Shouldn't happen in normal usage. */
class AFK_ClaimException: public std::exception {};

class AFK_Claimable
{
protected:
    /* The last time the object was seen. */
    AFK_Frame lastSeen;

    /* The last time the object was claimed exclusively. */
    AFK_Frame lastSeenExclusively;

#if CLAIMABLE_MUTEX
    boost::upgrade_mutex claimingMut;
#else
    /* Which thread ID (as assigned by the async module) has
     * claimed use of this object.  I'll increment it by 1 to
     * make sure no thread IDs of 0 are knocking about.
     * 0 means nobody.
     * 1<<63 is a special bit signifying "non-shared lock".
     */
    boost::atomic<uint64_t> claimingThreadId;
#endif

public:
    AFK_Claimable();

    /* Tries to claim this object for processing.
     * When finished, release it by calling release().
     */
    enum AFK_ClaimStatus claim(unsigned int threadId, enum AFK_ClaimType type, const AFK_Frame& currentFrame);

    /* Upgrades a shared claim to a non-shared one.
     * Again, call release() to finish.
     */
    enum AFK_ClaimStatus upgrade(unsigned int threadId, enum AFK_ClaimStatus status);

    void release(unsigned int threadId, enum AFK_ClaimStatus status);

    /* Helper -- tries a bit harder to claim the cell.
     * Returns the resulting status.
     */
    enum AFK_ClaimStatus claimYieldLoop(unsigned int threadId, enum AFK_ClaimType type, const AFK_Frame& currentFrame);

    const AFK_Frame& getLastSeen(void) const { return lastSeen; }
};

#endif /* _AFK_DATA_CLAIMABLE_H_ */

