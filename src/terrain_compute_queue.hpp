/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TERRAIN_COMPUTE_QUEUE_H_
#define _AFK_TERRAIN_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "terrain.hpp"

/* This module make something like a render list, but rather more
 * complicated, for the purpose of queueing up the terrain
 * computation tasks into a form suitable for shoveling straight
 * into the CL without further re-arranging in the compute
 * thread.
 * Sadly, it does have to be internally synchronized.
 * It's specialized right now, but I might want to consider
 * deriving something when I come to queueing up other
 * CL tasks.
 */

class AFK_TerrainComputeUnit
{
public:
    int tileOffset;
    int tileCount;
    Vec2<int> piece;

    AFK_TerrainComputeUnit(
        int _tileOffset,
        int _tileCount,
        const Vec2<int>& _piece);

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainComputeUnit& unit);
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_TerrainComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_TerrainComputeUnit>::value));

class AFK_TerrainComputeQueue: protected AFK_TerrainList
{
protected:
    /* In a TerrainComputeQueue, the TerrainList members are
     * actually a concatenated sequence of terrain features and
     * tiles.  The following vector describes each unit of
     * computation, allowing it to seek correctly within the
     * other two lists.
     */
    std::vector<AFK_TerrainComputeUnit> units;

    /* Used for internal synchronization, because the various
     * cell evaluator threads will be hitting a single one
     * of these.
     */
    boost::mutex mut;

public:
    /* Pushes a terrain list into the queue, making a Unit for it.
     * The Unit goes in too, but we return it as well so you can
     * instantly debug.
     */
    AFK_TerrainComputeUnit extend(const AFK_TerrainList& list, const Vec2<int>& piece, const AFK_LandscapeSizes& lSizes);

    /* This prints lots of debug info about the given terrain unit. */
    std::string debugTerrain(const AFK_TerrainComputeUnit& unit, const AFK_LandscapeSizes& lSizes) const;

    /* Call this with a `mem' array of size 3, for:
     * - features
     * - tiles
     * - units.
     */
    void copyToClBuffers(cl_context ctxt, cl_mem *mem);

    int getUnitCount(void);
    AFK_TerrainComputeUnit getUnit(int unitIndex);

    bool empty(void);

    /* Clears everything out ready for the next evaluation phase.
     * Make sure the CL tasks are finished first! :P
     */
    void clear(void);
};

#endif /* _AFK_TERRAIN_COMPUTE_QUEUE_H_ */

