/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_COMPUTE_QUEUE_H_
#define _AFK_LANDSCAPE_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <vector>

#include <boost/iterator/indirect_iterator.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "jigsaw.hpp"
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
protected:
    unsigned int tileOffset;
    unsigned int tileCount;
    unsigned int piece;

public:
    AFK_TerrainComputeUnit(
        unsigned int _tileOffset,
        unsigned int _tileCount,
        unsigned int _piece);
};

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
    AFK_TerrainComputeQueue::AFK_TerrainComputeQueue(const AFK_LandscapeSizes& lSizes);

    void extend(const AFK_TerrainList& list, unsigned int piece);

    /* Call this with a `mem' array of size 3, for:
     * - features
     * - tiles
     * - units.
     */
    void copyToClBuffers(cl_context ctxt, cl_mem *mem);

    unsigned int getUnitCount(void) const;
    AFK_TerrainComputeUnit getUnit(unsigned int unitIndex) const;

    /* Clears everything out ready for the next evaluation phase.
     * Make sure the CL tasks are finished first! :P
     */
    void clear(void);
};

/* In a similar vein to the render list, this keeps two series
 * of queues shadowing each other, one queue pair for each
 * puzzle in the jigsaw collection:
 * - The even numbered queues are filled on one frame and
 * emptied on the next
 * - The odd numbered queues alternate with them.
 *
 * Note that the collective noun for `queue' is clearly a
 * `fair'.  :-)
 */
class AFK_TerrainComputeFair
{
protected:
    std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> > queues;
    unsigned int updateInc; /* 0 or 1 */
    unsigned int drawInc; /* The opposite */

    boost::mutex mut; /* I'm afraid so */

public:
    AFK_TerrainComputeFair();

    /* Call when you're an evaluator thread holding a terrain
     * list and a jigsaw piece.  This method gives you a pointer
     * to the queue you should push them to.
     */
    boost::shared_ptr<AFK_TerrainComputeQueue> getUpdateQueue(
        const AFK_JigsawPiece& jigsawPiece,
        const AFK_LandscapeSizes& lSizes);

    /* Fills out the supplied vector with the queues to
     * draw on.
     */
    void getDrawQueues(std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> >& drawQueues);

    /* Swaps over the update and draw queues.  Call at the
     * end of a frame.
     */
    void flipQueues(void);
};

#endif /* _AFK_LANDSCAPE_COMPUTE_QUEUE_H_ */

