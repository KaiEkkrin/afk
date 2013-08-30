/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_3D_COMPUTE_QUEUE_H_
#define _AFK_3D_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <sstream>

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

class AFK_3DComputeUnit
{
public:
    /* Displacement and scale compared to the base cube. */
    Vec4<float> location;

    Vec3<int> vapourPiece;
    Vec2<int> edgePiece;
    
    /* Where the cube details are in the data stream.
     * TODO If I duplicate all the cubes for a large shape, there
     * will be a lot of overlap.  Do I want to instead include a
     * few words' worth of bitmask to tell the kernel which cubes
     * to include and which to ignore?
     */
    int cubeOffset;
    int cubeCount;

    AFK_3DComputeUnit();
    AFK_3DComputeUnit(
        const Vec4<float>& _location,
        const AFK_JigsawPiece& _vapourJigsawPiece,
        const AFK_JigsawPiece& _edgeJigsawPiece,
        int _cubeOffset,
        int _cubeCount);

    bool uninitialised(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DComputeUnit& unit);
};

std::ostream& operator<<(std::ostream& os, const AFK_3DComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_3DComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_3DComputeUnit>::value));

class AFK_3DComputeQueue: protected AFK_3DList
{
protected:
    /* Describes each unit of computation in sequence. */
    std::vector<AFK_3DComputeUnit> units;

    boost::mutex mut;

    cl_kernel vapourKernel, edgeKernel;
    
public:
    AFK_3DComputeQueue();
    virtual ~AFK_3DComputeQueue();

    /* Pushes a 3DList into the queue and makes a Unit for it
     * (which goes in too).
     */
    AFK_3DComputeUnit extend(
        const AFK_3DList& list,
        const AFK_ShapeCube& shapeCube,
        const AFK_ShapeSizes& sSizes);

    /* Makes a new Unit, re-using the given offsets.
     * TODO: This is here so that I can compute different
     C* cubes at various offsets within the same notional
     * vapour cloud.  If it becomes a limiting factor I
     * should change this to something that indirects the
     * 3D features list with a separate flag list for each
     * cube or something, to cut down on redundant
     * processing
     * TODO *2: Is this thing relevant at all ?
     */
    AFK_3DComputeUnit addUnitWithExisting(
        const AFK_3DComputeUnit& existingUnit,
        const AFK_ShapeCube& shapeCube);

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

