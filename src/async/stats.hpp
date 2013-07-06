/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_STATS_H_
#define _AFK_ASYNC_STATS_H_

#include <sstream>

#include <boost/atomic.h>

/* Useful tracking stats module for the async structures. */

class AFK_StructureStats
{
protected:
    /* The number of things in use in the structure. */
    boost::atomic<unsigned int> size;

    /* Accumulated contention: the number of times we
     * had to retry to slot a new thing.
     */
    boost::atomic<unsigned int> contention;

    /* The number of things that have been accumulated into
     * the `contention' value.  Reset to re-begin
     * contention sampling
     */
    boost::atomic<unsigned int> contentionSampleSize;

public:
    AFK_StructureStats();

    void insertedOne(unsigned int tries);
    void erasedOne(void);
    unsigned int getContentionAndReset(void);
    void printStats(std::ostream& os, const std::string& prefix) const;
};

#endif /* _AFK_ASYNC_STATS_H_ */

