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

#include <mutex>
#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_solid.hpp"
#include "compute_input_list.hpp"
#include "computer.hpp"
#include "core.hpp"
#include "def.hpp"
#include "dreduce.hpp"
#include "jigsaw_collection.hpp"
#include "keyed_cell.hpp"
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
} afk_align(16);

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_3DVapourComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_3DVapourComputeUnit>::value));

class AFK_3DVapourComputeQueue
{
protected:
    std::mutex mut;

    AFK_ComputeInputList<AFK_3DVapourFeature> featuresIn;
    AFK_ComputeInputList<AFK_3DVapourCube> cubesIn;
    AFK_ComputeInputList<AFK_3DVapourComputeUnit> unitsIn;
    cl_kernel vapourFeatureKernel;
    cl_kernel vapourNormalKernel;

    /* The number of vapour density and normal jigsaws we acquired... */
    int jpDCount, jpNCount;

    /* ...and the events we need to wait for before releasing them */
    AFK_ComputeDependency *preReleaseDep;

    AFK_DReduce *dReduce;

    /* Here we store the in-order list of source shape cell keys,
     * so that the dreduce can feed back its results easily.
     */
    std::vector<AFK_KeyedCell> shapeCells;
    
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
        unsigned int cubeCount,
        const AFK_KeyedCell& cell);

    void computeStart(
        AFK_Computer *computer,
        cl_mem vapourJigsawsDensityMem[4],
        cl_mem vapourJigsawsNormalMem[4],
        size_t vapourJigsawsCount,
        const Vec2<int>& fake3D_size,
        int fake3D_mult,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& o_postDep,
        const AFK_ShapeSizes& sSizes);

    void computeFinish(
        unsigned int threadId,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& o_postDep,
        AFK_SHAPE_CELL_CACHE *cache);

    /* To be part of a Fair. */
    bool empty(void);
    void clear(void);
};

#endif /* _AFK_3D_COMPUTE_QUEUE_H_ */

