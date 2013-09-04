/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_3D_EDGE_COMPUTE_QUEUE_H_
#define _AFK_3D_EDGE_COMPUTE_QUEUE_H_

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
 * 3D edge compute kernel.
 */

class AFK_3DEdgeComputeUnit
{
public:
    /* Displacement and scale compared to the base cube. */
    Vec4<float> location;

    /* TODO: This needs to become a full set of vapour adjacency
     * data.
     */
    Vec4<int> vapourPiece; /* a vec4 because OpenCL wants one to access a 3D texture */
    Vec2<int> edgePiece;

    AFK_3DEdgeComputeUnit(
        const Vec4<float>& _location,
        const AFK_JigsawPiece& _vapourJigsawPiece,
        const AFK_JigsawPiece& _edgeJigsawPiece);

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit);
} __attribute__((aligned(16)));

std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_3DEdgeComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_3DEdgeComputeUnit>::value));

class AFK_3DEdgeComputeQueue
{
protected:
    /* Describes each unit of computation in sequence. */
    std::vector<AFK_3DEdgeComputeUnit> units;

    boost::mutex mut;

    cl_kernel edgeKernel;

    std::vector<cl_event> preEdgeWaitList;
    std::vector<cl_event> postEdgeWaitList;
    
public:
    AFK_3DEdgeComputeQueue();
    virtual ~AFK_3DEdgeComputeQueue();

    /* Adds the edges of a shape cube to the queue. */
    AFK_3DEdgeComputeUnit append(const AFK_ShapeCube& shapeCube);

    /* Starts the computation.
     */
    void computeStart(
        AFK_Computer *computer,
        AFK_Jigsaw *vapourJigsaw,
        AFK_Jigsaw *edgeJigsaw,
        const AFK_ShapeSizes& sSizes);
    void computeFinish(void);

    /* To be part of a Fair. */
    bool empty(void);
    void clear(void);
};

#endif /* _AFK_3D_COMPUTE_QUEUE_H_ */

