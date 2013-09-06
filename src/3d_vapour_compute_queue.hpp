/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_3D_VAPOUR_COMPUTE_QUEUE_H_
#define _AFK_3D_VAPOUR_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "computer.hpp"
#include "def.hpp"
#include "jigsaw.hpp"
#include "shape.hpp"
#include "shape_sizes.hpp"

/* This module marshals 3D object compute data through the
 * 3D object compute kernels.
 */

class AFK_3DVapourComputeUnit
{
public:
    /* Displacement and scale compared to the base cube. */
    Vec4<float> location;

    Vec4<int> vapourPiece; /* a vec4 because OpenCL wants one to access a 3D texture */
    
    /* Where the cube details are in the data stream.
     * TODO If I duplicate all the cubes for a large shape, there
     * will be a lot of overlap.  Do I want to instead include a
     * few words' worth of bitmask to tell the kernel which cubes
     * to include and which to ignore?
     */
    int cubeOffset;
    int cubeCount;

    AFK_3DVapourComputeUnit();
    AFK_3DVapourComputeUnit(
        const Vec4<float>& _location,
        const AFK_JigsawPiece& _vapourJigsawPiece,
        int _cubeOffset,
        int _cubeCount);

    bool uninitialised(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DVapourComputeUnit& unit);
} __attribute__((aligned(16)));

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_3DVapourComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_3DVapourComputeUnit>::value));

class AFK_3DVapourComputeQueue: protected AFK_3DList
{
protected:
    /* Describes each unit of computation in sequence. */
    std::vector<AFK_3DVapourComputeUnit> units;

    boost::mutex mut;

    cl_kernel vapourKernel;

    std::vector<cl_event> preVapourWaitList;
    std::vector<cl_event> postVapourWaitList;
    
public:
    AFK_3DVapourComputeQueue();
    virtual ~AFK_3DVapourComputeQueue();

    /* Pushes a 3DList into the queue and makes a Unit for it
     * (which goes in too).
     */
    AFK_3DVapourComputeUnit extend(
        const AFK_3DList& list,
        const Vec4<float>& location,
        const AFK_JigsawPiece& vapourJigsawPiece,
        const AFK_ShapeSizes& sSizes);

    /* Makes a new compute unit based on an existing one
     * (so that I can re-use the offsets rather than having
     * to push the same data back into the queue).
     */
    AFK_3DVapourComputeUnit addUnitFromExisting(
        const AFK_3DVapourComputeUnit& existingUnit,
        const Vec4<float>& location,
        const AFK_JigsawPiece& vapourJigsawPiece);

    /* Starts computing this vapour.
     */
    void computeStart(
        AFK_Computer *computer,
        AFK_Jigsaw *vapourJigsaw,
        const AFK_ShapeSizes& sSizes);
    void computeFinish(void);

    /* To be part of a Fair. */
    bool empty(void);
    void clear(void);
};

#endif /* _AFK_3D_COMPUTE_QUEUE_H_ */

