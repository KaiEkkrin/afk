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

#ifndef _AFK_3D_EDGE_COMPUTE_QUEUE_H_
#define _AFK_3D_EDGE_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include <mutex>
#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "compute_input_list.hpp"
#include "computer.hpp"
#include "def.hpp"
#include "jigsaw.hpp"
#include "shape_sizes.hpp"

/* This module marshals 3D object compute data through the
 * 3D edge compute kernel.
 */

#define AFK_3DECU_VPCOUNT 7

#ifdef _WIN32
_declspec(align(16))
#endif
class AFK_3DEdgeComputeUnit
{
public:
    Vec4<int> vapourPiece;
    Vec2<int> edgePiece;

    AFK_3DEdgeComputeUnit(
        const AFK_JigsawPiece& _vapourJigsawPiece,
        const AFK_JigsawPiece& _edgeJigsawPiece);

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit);
}
#ifdef __GNUC__
__attribute__((aligned(16)))
#endif
;

std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_3DEdgeComputeUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_3DEdgeComputeUnit>::value));

class AFK_3DEdgeComputeQueue
{
protected:
    std::mutex mut;

    AFK_ComputeInputList<AFK_3DEdgeComputeUnit> unitsIn;
    cl_kernel edgeKernel;

    /* The events to wait for before we can release the vapour
     * and edges
     */
    AFK_ComputeDependency *postEdgeDep;
    
public:
    AFK_3DEdgeComputeQueue();
    virtual ~AFK_3DEdgeComputeQueue();

    /* Adds the edges of a shape cube to the queue. */
    AFK_3DEdgeComputeUnit append(
        const AFK_JigsawPiece& vapourJigsawPiece,
        const AFK_JigsawPiece& edgeJigsawPiece);

    /* Starts the computation.
     */
    void computeStart(
        AFK_Computer *computer,
        cl_mem vapourJigsawDensityMem,
        cl_mem edgeJigsawOverlapMem,
        const Vec2<int>& fake3D_size,
        int fake3D_mult,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& o_postDep,
        const AFK_ShapeSizes& sSizes);

#if 0
    void computeFinish(
        AFK_Jigsaw *vapourJigsaw,
        AFK_Jigsaw *edgeJigsaw);
#endif

    /* To be part of a Fair. */
    bool empty(void);
    void clear(void);
};

#endif /* _AFK_3D_COMPUTE_QUEUE_H_ */

