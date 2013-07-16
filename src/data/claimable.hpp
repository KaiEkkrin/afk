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

/* First here are the possible results from a claim attempt.
 */
enum AFK_ClaimStatus
{
    AFK_CL_CLAIMED,            /* You've got it */
    AFK_CL_ALREADY_PROCESSED,  /* It's already been processed this frame */
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

    /* Which thread ID (as assigned by the async module) has
     * claimed use of this object.
     */
    boost::atomic<unsigned int> claimingThreadId;

public:
    AFK_Claimable();

    /* Tries to claim this object for processing.
     * When finished, release it by calling release().
     * The flag says whether to update the lastSeen field:
     * only one claim can do this per frame, so claims for
     * spillage or eviction shouldn't set this.
     * TODO Can I remove the `touch' flag now?
     */
    enum AFK_ClaimStatus claim(unsigned int threadId, bool touch);

    void release(unsigned int threadId);

    /* Helper -- tries a bit harder to claim the cell.
     * Returns true if success, else false
     */
    bool claimYieldLoop(unsigned int threadId, bool touch);

    /* Things the implementer needs to define. */
    virtual AFK_Frame getCurrentFrame(void) const = 0;
    virtual bool canBeEvicted(void) const = 0;
};

#endif /* _AFK_DATA_CLAIMABLE_H_ */

