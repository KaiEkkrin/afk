/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_CLAIMABLE_H_
#define _AFK_DATA_CLAIMABLE_H_

#include <exception>

#include <boost/atomic.hpp>

#include "frame.hpp"

/* A "Claimable" data item is one that can be claim()'d and
 * release()'d like a poor man's lock, optionally stamping it
 * with the frame time at which this occurred.
 * Thus, Claimables can be safely used in evicting caches,
 * because the evictor can check they haven't been used for
 * a few frames before taking them out.
 */

/* TODO: Should Claimable just use a mutex instead?  I mean,
 * that's what it's doing, and it's actually behaviour that
 * I want.  And I'm pretty sure yield() goes to system.
 * Nngghh! :)
 */

/* Here are the different ways in which we can claim a cell. */
enum AFK_ClaimType
{
    AFK_CLT_EXCLUSIVE,          /* Wants to be the only claim to processing the cell this frame */
    AFK_CLT_NONEXCLUSIVE,       /* Bump the frame, but other threads may claim it this frame after
                                 * we've released
                                 */
    AFK_CLT_EVICTOR             /* We're the evictor.  Don't bump the frame. */
};

/* First here are the possible results from a claim attempt.
 */
enum AFK_ClaimStatus
{
    AFK_CL_CLAIMED,            /* You've got it */
    AFK_CL_ALREADY_PROCESSED,  /* It's already been processed this frame, and you wanted an
                                * exclusive claim
                                */
    AFK_CL_TAKEN               /* Someone else has it */
};

/* This is the "unclaimed" thread ID. */
#define UNCLAIMED 0xffffffffu

/* For errors.  Shouldn't happen in normal usage. */
class AFK_ClaimException: public std::exception {};

class AFK_Claimable
{
protected:
    /* The last time the object was seen. */
    AFK_Frame lastSeen;

    /* The last time the object was claimed exclusively. */
    AFK_Frame lastSeenExclusively;

    /* Which thread ID (as assigned by the async module) has
     * claimed use of this object.
     */
    boost::atomic<unsigned int> claimingThreadId;

public:
    AFK_Claimable();

    /* Tries to claim this object for processing.
     * When finished, release it by calling release().
     */
    enum AFK_ClaimStatus claim(unsigned int threadId, enum AFK_ClaimType type);

    void release(unsigned int threadId);

    /* Helper -- tries a bit harder to claim the cell.
     * Returns true if success, else false
     */
    bool claimYieldLoop(unsigned int threadId, enum AFK_ClaimType type);

    /* Things the implementer needs to define. */
    virtual AFK_Frame getCurrentFrame(void) const = 0;
    virtual bool canBeEvicted(void) const = 0;
};

#endif /* _AFK_DATA_CLAIMABLE_H_ */

