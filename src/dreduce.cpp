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

#include "dreduce.hpp"
#include "shape_cell.hpp"

/* AFK_DReduce implementation */

AFK_DReduce::AFK_DReduce(AFK_Computer *_computer):
    computer(_computer), buf(0), bufSize(0), readback(nullptr), readbackSize(0), readbackDep(computer)
{
    if (!computer->findKernel("makeShape3DVapourDReduce", dReduceKernel))
        throw AFK_Exception("Cannot find D-reduce kernel");
}

AFK_DReduce::~AFK_DReduce()
{
    if (buf) computer->oclShim.ReleaseMemObject()(buf);
    if (readback) delete[] readback;
}

void AFK_DReduce::compute(
    size_t unitCount,
    cl_mem *units,
    const Vec2<int>& fake3D_size,
    int fake3D_mult,
    cl_mem *vapourJigsawsDensityMem,
    const AFK_ShapeSizes& sSizes,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    cl_int error;

    cl_context ctxt = computer->getContext();
    auto kernelQueue = computer->getKernelQueue();
    auto readQueue = computer->getReadQueue();

    size_t requiredSize = unitCount * 2 * sizeof(float);
    if (bufSize < requiredSize)
    {
        if (buf)
            AFK_CLCHK(computer->oclShim.ReleaseMemObject()(buf))

        buf = computer->oclShim.CreateBuffer()(
            ctxt, CL_MEM_WRITE_ONLY,
            requiredSize,
            nullptr,
            &error);
        AFK_HANDLE_CL_ERROR(error);
        bufSize = requiredSize;
    }

    kernelQueue->kernel(dReduceKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), units);
    kernelQueue->kernelArg(sizeof(cl_int2), &fake3D_size.v[0]);
    kernelQueue->kernelArg(sizeof(cl_int), &fake3D_mult);

    for (int vfI = 0; vfI < 4; ++vfI)
        kernelQueue->kernelArg(sizeof(cl_mem), &vapourJigsawsDensityMem[vfI]);

    kernelQueue->kernelArg(sizeof(cl_mem), &buf);

    size_t dReduceGlobalDim[3];
    dReduceGlobalDim[0] = unitCount;
    dReduceGlobalDim[1] = dReduceGlobalDim[2] = (1uLL << sSizes.getReduceOrder());

    size_t dReduceLocalDim[3];
    dReduceLocalDim[0] = 1;
    dReduceLocalDim[1] = dReduceLocalDim[2] = (1uLL << sSizes.getReduceOrder());

    kernelQueue->kernel3D(dReduceGlobalDim, dReduceLocalDim, preDep, o_postDep);

    size_t requiredReadbackSize = requiredSize / sizeof(float);
    if (readbackSize < requiredReadbackSize)
    {
        if (readback) delete[] readback;
        readback = new float[requiredReadbackSize];
        readbackSize = requiredReadbackSize;
    }

    readQueue->readBuffer(buf, requiredSize, readback, o_postDep, readbackDep);
}

void AFK_DReduce::readBack(
    unsigned int threadId,
    size_t unitCount,
    const std::vector<AFK_KeyedCell>& shapeCells,
    AFK_SHAPE_CELL_CACHE *cache)
{
    readbackDep.waitFor();

    /* TODO: Here's that logic again, as in yreduce...
     * algorithm-ify?
     */
    bool allPushed = false;
    std::vector<bool> pushed;
    pushed.reserve(shapeCells.size());
    for (size_t i = 0; i < shapeCells.size(); ++i) pushed.push_back(false);

    do
    {
        allPushed = true;
        for (size_t i = 0; i < shapeCells.size(); ++i)
        {
            if (pushed[i]) continue;

            auto entry = cache->get(threadId, shapeCells[i]);
            if (entry)
            {
                auto claim = entry->claimable.claim(threadId, 0);
                if (claim.isValid())
                {
                    claim.get().setDMinMax(readback[i * 2], readback[i * 2 + 1]);
                    pushed[i] = true;
                }
                else
                {
                    /* Want to retry */
                    allPushed = false;
                }
            }
            else
            {
                /* Ignore, no entry any more */
                pushed[i] = true;
            }
        }

        if (!allPushed) std::this_thread::yield();
    } while (!allPushed);
}

