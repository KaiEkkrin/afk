/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include <cassert>

#include "3d_vapour_compute_queue.hpp"
#include "compute_dependency.hpp"
#include "compute_queue.hpp"
#include "debug.hpp"
#include "exception.hpp"


/* AFK_3DVapourComputeUnit implementation */

AFK_3DVapourComputeUnit::AFK_3DVapourComputeUnit():
    cubeOffset(-1)
{
}

AFK_3DVapourComputeUnit::AFK_3DVapourComputeUnit(
    const Vec4<float>& _location,
    const Vec4<float>& _baseColour,
    const AFK_JigsawPiece& _vapourJigsawPiece,
    int _adjacencies,
    int _cubeOffset,
    int _cubeCount):
        location(_location),
        baseColour(_baseColour),
        adjacencies(_adjacencies),
        cubeOffset(_cubeOffset),
        cubeCount(_cubeCount),
        padding(0)
{
	vapourPiece = afk_vec4<int>(
		_vapourJigsawPiece.u,
		_vapourJigsawPiece.v,
		_vapourJigsawPiece.w,
		_vapourJigsawPiece.puzzle);
}

bool AFK_3DVapourComputeUnit::uninitialised(void) const
{
    return (cubeOffset < 0);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << afk_vec4<float, cl_float4>(unit.location);
    os << ", baseColour=" << std::dec << afk_vec4<float, cl_float4>(unit.baseColour);
    os << ", adjacencies=" << std::hex << unit.adjacencies;
    os << ", cubeOffset=" << std::dec << unit.cubeOffset;
    os << ", cubeCount=" << std::dec << unit.cubeCount;
    os << ", vapourPiece=" << std::dec << afk_vec4<int, cl_int4>(unit.vapourPiece);
    os << ")";
    return os;
}


/* AFK_3DVapourComputeQueue implementation */

AFK_3DVapourComputeQueue::AFK_3DVapourComputeQueue():
    vapourFeatureKernel(0),
    vapourNormalKernel(0),
    preReleaseDep(nullptr),
    dReduce(nullptr)
{
}

AFK_3DVapourComputeQueue::~AFK_3DVapourComputeQueue()
{
    if (dReduce) delete dReduce;
    if (preReleaseDep) delete preReleaseDep;
}

void AFK_3DVapourComputeQueue::extend(
    const AFK_3DList& list,
    unsigned int& o_cubeOffset,
    unsigned int& o_cubeCount)
{
    std::unique_lock<std::mutex> lock(mut);

    /* Sanity checks */
    assert(list.cubeCount() > 0);

    /* Avoiding massive 64-bit offset values here */
    o_cubeOffset = static_cast<unsigned int>(AFK_3DList::cubeCount());
    o_cubeCount = static_cast<unsigned int>(list.cubeCount());
    AFK_3DList::extend(list);
}

AFK_3DVapourComputeUnit AFK_3DVapourComputeQueue::addUnit(
    const Vec4<float>& location,
    const Vec4<float>& baseColour,
    const AFK_JigsawPiece& vapourJigsawPiece,
    int adjacencies,
    unsigned int cubeOffset,
    unsigned int cubeCount,
    const AFK_KeyedCell& cell)
{
    std::unique_lock<std::mutex> lock(mut);

    /* Sanity checks */
    assert(cubeOffset >= 0 && cubeOffset < c.size());
    assert(cubeCount > 0);
    assert((cubeOffset + cubeCount) <= c.size());

    AFK_3DVapourComputeUnit newUnit(
        location,
        baseColour,
        vapourJigsawPiece,
        adjacencies,
        cubeOffset,
        cubeCount);
    units.push_back(newUnit);
    shapeCells.push_back(cell);
    return newUnit;
}

void AFK_3DVapourComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_JigsawCollection *vapourJigsaws,
    const AFK_ShapeSizes& sSizes)
{
    /* TODO: Progressively re-activate this and the next method and debug. */
    std::unique_lock<std::mutex> lock(mut);

    /* Check there's something to do */
    size_t unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!vapourFeatureKernel)
        if (!computer->findKernel("makeShape3DVapourFeature", vapourFeatureKernel))
            throw AFK_Exception("Cannot find 3D vapour feature kernel");

    if (!vapourNormalKernel)
        if (!computer->findKernel("makeShape3DVapourNormal", vapourNormalKernel))
            throw AFK_Exception("Cannot find 3D vapour normal kernel");

    if (!dReduce)
        dReduce = new AFK_DReduce(computer);

    auto kernelQueue = computer->getKernelQueue();
    auto writeQueue = computer->getWriteQueue();

    /* Make sure the end dependency is a thing */
    if (!preReleaseDep) preReleaseDep = new AFK_ComputeDependency(computer);
    assert(preReleaseDep->getEventCount() == 0);

    /* Copy the vapour inputs to CL buffers. */
    AFK_ComputeDependency noDep(computer);
    AFK_ComputeDependency preVapourDep(computer);

    cl_mem vapourBufs[3] = {
        featureInput.bufferData(f.data(), f.size() * sizeof(AFK_3DVapourFeature), computer, noDep, preVapourDep),
        cubeInput.bufferData(c.data(), c.size() * sizeof(AFK_3DVapourCube), computer, noDep, preVapourDep),
        unitInput.bufferData(units.data(), units.size() * sizeof(AFK_3DVapourComputeUnit), computer, noDep, preVapourDep)
    };

    /* Set up the rest of the vapour parameters */
    cl_mem vapourJigsawsDensityMem[4];
    Vec2<int> fake3D_size;
    int fake3D_mult;
    jpDCount = vapourJigsaws->acquireAllForCl(computer, 0, vapourJigsawsDensityMem, 4, fake3D_size, fake3D_mult, preVapourDep);

    kernelQueue->kernel(vapourFeatureKernel);

    for (int vbI = 0; vbI < 3; ++vbI)
        kernelQueue->kernelArg(sizeof(cl_mem), &vapourBufs[vbI]);

    kernelQueue->kernelArg(sizeof(cl_int2), &fake3D_size.v[0]);
    kernelQueue->kernelArg(sizeof(cl_int), &fake3D_mult);

    for (int jpI = 0; jpI < 4; ++jpI)
        kernelQueue->kernelArg(sizeof(cl_mem), &vapourJigsawsDensityMem[jpI]);

    size_t vapourDim[3];
    vapourDim[0] = sSizes.tDim * unitCount;
    vapourDim[1] = vapourDim[2] = sSizes.tDim;

    AFK_ComputeDependency preNormalDep(computer);
    kernelQueue->kernel3D(vapourDim, nullptr, preVapourDep, preNormalDep);

    /* Next, compute the vapour normals. */
    cl_mem vapourJigsawsNormalMem[4];
    jpNCount = vapourJigsaws->acquireAllForCl(computer, 1, vapourJigsawsNormalMem, 4, fake3D_size, fake3D_mult, preNormalDep);
    assert(jpNCount == jpDCount);

    kernelQueue->kernel(vapourNormalKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), &vapourBufs[2]);
    kernelQueue->kernelArg(sizeof(cl_int2), &fake3D_size.v[0]);
    kernelQueue->kernelArg(sizeof(cl_int), &fake3D_mult);

    for (int jpI = 0; jpI < 4; ++jpI)
        kernelQueue->kernelArg(sizeof(cl_mem), &vapourJigsawsDensityMem[jpI]);

    for (int jpI = 0; jpI < 4; ++jpI)
        kernelQueue->kernelArg(sizeof(cl_mem), &vapourJigsawsNormalMem[jpI]);

    kernelQueue->kernel3D(vapourDim, nullptr, preNormalDep, *preReleaseDep);

    /* While we're doing that, also enqueue the D reduce. */
    dReduce->compute(
        unitCount,
        &vapourBufs[2],
        fake3D_size,
        fake3D_mult,
        vapourJigsawsDensityMem,
        sSizes,
        preNormalDep,
        *preReleaseDep);
}

void AFK_3DVapourComputeQueue::computeFinish(unsigned int threadId, AFK_JigsawCollection *vapourJigsaws, AFK_SHAPE_CELL_CACHE *cache)
{
    std::unique_lock<std::mutex> lock(mut);

    size_t unitCount = units.size();
    if (unitCount == 0) return;

    assert(preReleaseDep);
    vapourJigsaws->releaseAllFromCl(0, jpDCount, *preReleaseDep);
    vapourJigsaws->releaseAllFromCl(1, jpNCount, *preReleaseDep);

    /* Read back the D reduce. */
    assert(dReduce);
    dReduce->readBack(threadId, unitCount, shapeCells, cache);
}

bool AFK_3DVapourComputeQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return units.empty();
}

void AFK_3DVapourComputeQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    if (preReleaseDep) preReleaseDep->waitFor();

    f.clear();
    c.clear();
    units.clear();
    shapeCells.clear();
}

