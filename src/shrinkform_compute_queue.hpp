/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHRINKFORM_COMPUTE_QUEUE_H_
#define _AFK_SHRINKFORM_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <sstream>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "computer.hpp"
#include "def.hpp"
#include "jigsaw.hpp"
#include "shape_sizes.hpp"
#include "shrinkform.hpp"

/* This module marshals shrinkform data into the shrinkform
 * compute kernel, just like terrain_compute_queue for the
 * terrain.
 */

class AFK_ShrinkformComputeUnit
{
public:
    int cubeOffset;
    int cubeCount;
    Vec2<int> piece;

    AFK_ShrinkformComputeUnit(
        int _cubeOffset,
        int _cubeCount,
        const Vec2<int>& _piece);

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformComputeUnit& unit);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_ShrinkformComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_ShrinkformComputeUnit>::value));

class AFK_ShrinkformComputeQueue: protected AFK_ShrinkformList
{
protected:
    /* Describes each unit of computation in sequence. */
    std::vector<AFK_ShrinkformComputeUnit> units;

    boost::mutex mut;

    cl_kernel shrinkformKernel, surfaceKernel;
    
public:
    AFK_ShrinkformComputeQueue();
    virtual ~AFK_ShrinkformComputeQueue();

    /* Pushes a ShrinkformList into the queue and makes a Unit for it
     * (which goes in too).
     */
    AFK_ShrinkformComputeUnit extend(const AFK_ShrinkformList& list, const Vec2<int>& piece, const AFK_ShapeSizes& sSizes);

    void computeStart(AFK_Computer *computer, AFK_Jigsaw *jigsaw, const AFK_ShapeSizes& sSizes);
    void computeFinish(void);

    /* To be part of a Fair. */
    bool empty(void);
    void clear(void);
};

#endif /* _AFK_SHRINKFORM_COMPUTE_QUEUE_H_ */

