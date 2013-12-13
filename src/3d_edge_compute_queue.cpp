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
#include <cassert>

#include "3d_edge_compute_queue.hpp"
#include "compute_queue.hpp"
#include "debug.hpp"
#include "exception.hpp"


/* AFK_3DEdgeComputeUnit implementation */

AFK_3DEdgeComputeUnit::AFK_3DEdgeComputeUnit(
    const AFK_JigsawPiece& _vapourJigsawPiece,
    const AFK_JigsawPiece& _edgeJigsawPiece)
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
    os << "vapourPiece=" << unit.vapourPiece;
    os << ", edgePiece=" << unit.edgePiece;
    os << ")";
    return os;
}


/* AFK_3DEdgeComputeQueue implementation */

AFK_3DEdgeComputeQueue::AFK_3DEdgeComputeQueue():
    edgeKernel(0), postEdgeDep(nullptr)
{
}

AFK_3DEdgeComputeQueue::~AFK_3DEdgeComputeQueue()
{
    if (postEdgeDep) delete postEdgeDep;
}

AFK_3DEdgeComputeUnit AFK_3DEdgeComputeQueue::append(
    const AFK_JigsawPiece& vapourJigsawPiece,
    const AFK_JigsawPiece& edgeJigsawPiece)
{
    std::unique_lock<std::mutex> lock(mut);

    AFK_3DEdgeComputeUnit newUnit(
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
    std::unique_lock<std::mutex> lock(mut);

    /* Check there's something to do */
    size_t unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!edgeKernel)
        if (!computer->findKernel("makeShape3DEdge", edgeKernel))
            throw AFK_Exception("Cannot find 3D edge kernel");

    auto kernelQueue = computer->getKernelQueue();
    auto writeQueue = computer->getWriteQueue();

    /* Copy the unit list to a CL buffer. */
    AFK_ComputeDependency noDep(computer);
    AFK_ComputeDependency preEdgeDep(computer);
    
    cl_mem unitsBuf = writeQueue->newReadOnlyBuffer(
        units.data(), units.size() * sizeof(AFK_3DEdgeComputeUnit), noDep, preEdgeDep);

    /* Set up the rest of the parameters */
    if (!postEdgeDep) postEdgeDep = new AFK_ComputeDependency(computer);
    assert(postEdgeDep->getEventCount() == 0);

    vapourJigsaw->setupImages(computer);
    edgeJigsaw->setupImages(computer);

    cl_mem vapourJigsawDensityMem = vapourJigsaw->acquireForCl(0, preEdgeDep);
    cl_mem edgeJigsawOverlapMem = edgeJigsaw->acquireForCl(0, preEdgeDep);

    kernelQueue->kernel(edgeKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), &vapourJigsawDensityMem);
    kernelQueue->kernelArg(sizeof(cl_mem), &unitsBuf);

    /* Note -- assuming all vapour shares a size for now
     * (just like in 3d_vapour_compute_queue)
     * If that changes the assert() in that module should remind
     * me to make suitable edits.
     */
    Vec2<int> fake3D_size = vapourJigsaw->getFake3D_size(0);
    int fake3D_mult = vapourJigsaw->getFake3D_mult(0);
    kernelQueue->kernelArg(sizeof(cl_int2), &fake3D_size.v[0]);
    kernelQueue->kernelArg(sizeof(cl_int), &fake3D_mult);

    kernelQueue->kernelArg(sizeof(cl_mem), &edgeJigsawOverlapMem);

    size_t edgeGlobalDim[3];
    edgeGlobalDim[0] = unitCount;
    edgeGlobalDim[1] = edgeGlobalDim[2] = sSizes.eDim;

    size_t edgeLocalDim[3];
    edgeLocalDim[0] = 1;
    edgeLocalDim[1] = edgeLocalDim[2] = sSizes.eDim;

    kernelQueue->kernel3D(edgeGlobalDim, edgeLocalDim, preEdgeDep, *postEdgeDep);
    AFK_CLCHK(computer->oclShim.ReleaseMemObject()(unitsBuf))
}

void AFK_3DEdgeComputeQueue::computeFinish(
    AFK_Jigsaw *vapourJigsaw,
    AFK_Jigsaw *edgeJigsaw)
{
    assert(postEdgeDep || units.size() == 0);
    if (units.size() > 0)
    {
        vapourJigsaw->releaseFromCl(0, *postEdgeDep);
        edgeJigsaw->releaseFromCl(0, *postEdgeDep);
    }
}

bool AFK_3DEdgeComputeQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return units.empty();
}

void AFK_3DEdgeComputeQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    if (postEdgeDep) postEdgeDep->waitFor();
    units.clear();
}

