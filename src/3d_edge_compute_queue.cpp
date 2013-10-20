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

#include "afk.hpp"

#include <algorithm>

#include "3d_edge_compute_queue.hpp"
#include "exception.hpp"


/* AFK_3DEdgeComputeUnit implementation */

AFK_3DEdgeComputeUnit::AFK_3DEdgeComputeUnit(
    const Vec4<float>& _location,
    const AFK_JigsawPiece& _vapourJigsawPiece,
    const AFK_JigsawPiece& _edgeJigsawPiece):
        location(_location)
{
    vapourPiece = afk_vec4<int>(
        _vapourJigsawPiece.u,
        _vapourJigsawPiece.v,
        _vapourJigsawPiece.w,
        _vapourJigsawPiece.puzzle);

	edgePiece = afk_vec2<int>(
		_edgeJigsawPiece.u,
		_edgeJigsawPiece.v);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << unit.location;
    os << ", vapourPiece=" << unit.vapourPiece;
    os << ", edgePiece=" << unit.edgePiece;
    os << ")";
    return os;
}


/* AFK_3DEdgeComputeQueue implementation */

AFK_3DEdgeComputeQueue::AFK_3DEdgeComputeQueue():
    edgeKernel(0)
{
}

AFK_3DEdgeComputeQueue::~AFK_3DEdgeComputeQueue()
{
}

AFK_3DEdgeComputeUnit AFK_3DEdgeComputeQueue::append(
    const Vec4<float>& location,
    const AFK_JigsawPiece& vapourJigsawPiece,
    const AFK_JigsawPiece& edgeJigsawPiece)
{
    boost::unique_lock<boost::mutex> lock(mut);

    AFK_3DEdgeComputeUnit newUnit(
        location,
        vapourJigsawPiece,
        edgeJigsawPiece);
    units.push_back(newUnit);
    return newUnit;
}

void AFK_3DEdgeComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_Jigsaw *vapourJigsaw,
    AFK_Jigsaw *edgeJigsaw,
    const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!edgeKernel)
        if (!computer->findKernel("makeShape3DEdge", edgeKernel))
            throw AFK_Exception("Cannot find 3D edge kernel");

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the unit list to a CL buffer. */
    cl_mem unitsBuf = computer->oclShim.CreateBuffer()(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_3DEdgeComputeUnit),
        &units[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    /* Set up the rest of the parameters */
    preEdgeWaitList.clear();
    cl_mem vapourJigsawDensityMem = vapourJigsaw->acquireForCl(0, preEdgeWaitList);
    cl_mem edgeJigsawDispMem = edgeJigsaw->acquireForCl(0, preEdgeWaitList);
    cl_mem edgeJigsawOverlapMem = edgeJigsaw->acquireForCl(1, preEdgeWaitList);

    AFK_CLCHK(computer->oclShim.SetKernelArg()(edgeKernel, 0, sizeof(cl_mem), &vapourJigsawDensityMem))
    AFK_CLCHK(computer->oclShim.SetKernelArg()(edgeKernel, 1, sizeof(cl_mem), &unitsBuf))

    /* Note -- assuming all vapour shares a size for now
     * (just like in 3d_vapour_compute_queue)
     * If that changes the assert() in that module should remind
     * me to make suitable edits.
     */
    Vec2<int> fake3D_size = vapourJigsaw->getFake3D_size(0);
    int fake3D_mult = vapourJigsaw->getFake3D_mult(0);
    AFK_CLCHK(computer->oclShim.SetKernelArg()(edgeKernel, 2, sizeof(cl_int2), &fake3D_size.v[0]))
    AFK_CLCHK(computer->oclShim.SetKernelArg()(edgeKernel, 3, sizeof(cl_int), &fake3D_mult))

    AFK_CLCHK(computer->oclShim.SetKernelArg()(edgeKernel, 4, sizeof(cl_mem), &edgeJigsawDispMem))
    AFK_CLCHK(computer->oclShim.SetKernelArg()(edgeKernel, 5, sizeof(cl_mem), &edgeJigsawOverlapMem))

    size_t edgeGlobalDim[3];
    edgeGlobalDim[0] = unitCount;
    edgeGlobalDim[1] = edgeGlobalDim[2] = sSizes.eDim;

    size_t edgeLocalDim[3];
    edgeLocalDim[0] = 1;
    edgeLocalDim[1] = edgeLocalDim[2] = sSizes.eDim;

    cl_event edgeEvent;

    AFK_CLCHK(computer->oclShim.EnqueueNDRangeKernel()(q, edgeKernel, 3, 0, &edgeGlobalDim[0],
        &edgeLocalDim[0],
        preEdgeWaitList.size(),
        &preEdgeWaitList[0],
        &edgeEvent))

    for (auto ev : preEdgeWaitList)
    {
        AFK_CLCHK(computer->oclShim.ReleaseEvent()(ev))
    }

    AFK_CLCHK(computer->oclShim.ReleaseMemObject()(unitsBuf))

    postEdgeWaitList.clear();
    postEdgeWaitList.push_back(edgeEvent);
    vapourJigsaw->releaseFromCl(0, postEdgeWaitList);
    edgeJigsaw->releaseFromCl(0, postEdgeWaitList);
    edgeJigsaw->releaseFromCl(1, postEdgeWaitList);
    AFK_CLCHK(computer->oclShim.ReleaseEvent()(edgeEvent))

    computer->unlock();
}

void AFK_3DEdgeComputeQueue::computeFinish(void)
{
    /* This method is included for symmetry with TerrainComputeQueue.
     * Right now it does nothing.
     */
}

bool AFK_3DEdgeComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
}

void AFK_3DEdgeComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    units.clear();
}

