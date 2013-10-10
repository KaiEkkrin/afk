/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_3D_VAPOUR_COMPUTE_QUEUE_H_
#define _AFK_3D_VAPOUR_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_solid.hpp"
#include "computer.hpp"
#include "def.hpp"
#include "jigsaw_collection.hpp"
#include "shape_sizes.hpp"

/* This module marshals 3D object compute data through the
 * 3D object compute kernels.
 */

class AFK_3DVapourComputeUnit
{
public:
    /* Displacement and scale compared to the base cube. */
    Vec4<float> location;

    Vec4<float> baseColour;

    Vec4<int> vapourPiece; /* a vec4 because OpenCL wants one to access a 3D texture */

    int adjacencies; /* Bitmask: for faces 0-5 inclusive,
                      * whether or not the skeleton continues
                      * from that adjacent face.
                      */
    
    /* Where the cube details are in the data stream.
     * TODO If I duplicate all the cubes for a large shape, there
     * will be a lot of overlap.  Do I want to instead include a
     * few words' worth of bitmask to tell the kernel which cubes
     * to include and which to ignore?
     * Perhaps when I do the full/empty vapour feedback I could also
     * feed back stats on how many of the features were within range,
     * to find out how much optimisation is possible here.
     */
    int cubeOffset;
    int cubeCount;

    AFK_3DVapourComputeUnit();
    AFK_3DVapourComputeUnit(
        const Vec4<float>& _location,
        const Vec4<float>& _baseColour,
        const AFK_JigsawPiece& _vapourJigsawPiece,
        int _adjacencies,
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

    /* Pushes a 3DList into the queue and fills out the
     * `cubeOffset' and `cubeCount' parameters, which are
     * needed to push in a compute unit.
     * (You'll want several compute units to share the same
     * offsets, I'm sure.)
     */
    void extend(
        const AFK_3DList& list,
        unsigned int& o_cubeOffset,
        unsigned int& o_cubeCount);

    /* Pushes a new compute unit into the queue using the
     * offsets you got earlier.
     * (And returns it.)
     */
    AFK_3DVapourComputeUnit addUnit(
        const Vec4<float>& location,
        const Vec4<float>& baseColour,
        const AFK_JigsawPiece& vapourJigsawPiece,
        int adjacencies,
        unsigned int cubeOffset,
        unsigned int cubeCount);

    /* Starts computing the vapour.
     */
    void computeStart(
        AFK_Computer *computer,
        AFK_JigsawCollection *vapourJigsaws,
        const AFK_ShapeSizes& sSizes);
    void computeFinish(void);

    /* To be part of a Fair. */
    bool empty(void);
    void clear(void);
};

#endif /* _AFK_3D_COMPUTE_QUEUE_H_ */

