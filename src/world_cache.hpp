/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_CACHE_H_
#define _AFK_WORLD_CACHE_H_

#include "afk.hpp"

#include "data/polymer_cache.hpp"
#include "world.hpp"
#include "world_cell.hpp"

/* The world cache.
 * I'm no longer supporting the map cache -- I'm confident that
 * my polymer cache is OK.  Really.  (Nnnnnngggg scared, Mummy!)
 */
class AFK_WorldCache: public AFK_PolymerCache<AFK_Cell, AFK_WorldCell, AFK_HashCell>
{
protected:
    /* The state of the evictor. */
    size_t targetSize;
    size_t kickoffSize;

    boost::thread *th;
    boost::promise<unsigned int> *rp;
    boost::unique_future<unsigned int> result;

    bool stop;

    /* Stats. */
    unsigned int entriesEvicted;
    unsigned int runsSkipped;
    unsigned int runsOverlapped;

    void evictionWorker(void);

public:
    AFK_WorldCache(size_t _targetSize);
    virtual ~AFK_WorldCache();

    virtual void doEvictionIfNecessary(void);
    virtual void printStats(std::ostream& os, const std::string& prefix) const;

    bool withinTargetSize(void) const;
};

#endif /* _AFK_WORLD_CACHE_H_ */

