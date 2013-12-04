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

#include <thread>

#include "core.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "world.hpp"
#include "yreduce.hpp"

AFK_YReduce::AFK_YReduce(AFK_Computer *_computer):
    computer(_computer), buf(0), bufSize(0), readback(nullptr), readbackSize(0), readbackDep(computer)
{
    if (!computer->findKernel("makeLandscapeYReduce", yReduceKernel))
        throw AFK_Exception("Cannot find Y-reduce kernel");
}

AFK_YReduce::~AFK_YReduce()
{
    if (buf) computer->oclShim.ReleaseMemObject()(buf);
    if (readback) delete[] readback;
}

void AFK_YReduce::compute(
    size_t unitCount,
    cl_mem *units,
    cl_mem *jigsawYDisp,
    cl_sampler *yDispSampler,
    const AFK_LandscapeSizes& lSizes,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    cl_int error;

    cl_context ctxt = computer->getContext();
    auto kernelQueue = computer->getKernelQueue();
    auto readQueue = computer->getReadQueue();

    /* The buffer needs to be big enough for 2 floats per
     * unit.
     */
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

    kernelQueue->kernel(yReduceKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), units);
    kernelQueue->kernelArg(sizeof(cl_mem), jigsawYDisp);
    kernelQueue->kernelArg(sizeof(cl_sampler), yDispSampler);
    kernelQueue->kernelArg(sizeof(cl_mem), &buf);

    size_t yReduceGlobalDim[2];
    yReduceGlobalDim[0] = (1uLL << lSizes.getReduceOrder());
    yReduceGlobalDim[1] = unitCount;

    size_t yReduceLocalDim[2];
    yReduceLocalDim[0] = (1uLL << lSizes.getReduceOrder());
    yReduceLocalDim[1] = 1;

    kernelQueue->kernel2D(yReduceGlobalDim, yReduceLocalDim, preDep, o_postDep);

    size_t requiredReadbackSize = requiredSize / sizeof(float);
    if (readbackSize < requiredReadbackSize)
    {
        if (readback) delete[] readback;
        readback = new float[requiredReadbackSize];
        readbackSize = requiredReadbackSize;
    }

    readQueue->readBuffer(buf, requiredSize, readback, o_postDep, readbackDep);
}

void AFK_YReduce::readBack(
    unsigned int threadId,
    size_t unitCount,
    const std::vector<AFK_Tile>& landscapeTiles,
    AFK_LANDSCAPE_CACHE *cache)
{
    readbackDep.waitFor();
#if 0
    std::cout << "Computed y bounds: ";
    for (unsigned int i = 0; i < 4 && i < unitCount; ++i)
    {
        if (i > 0) std::cout << ", ";
        std::cout << "(" << readback[i * 2] << ", " << readback[i * 2 + 1] << ")";
    }
    std::cout << std::endl;
#endif

    /* Push all those tile bounds into the cache, so long as an
     * entry can be found.
     * TODO This logic is almost the same as that found in
     * landscape_display_queue to cull the draw list.  Can I make
     * it into a template algorithm?
     */
    bool allPushed = false;
    std::vector<bool> pushed;
    pushed.reserve(landscapeTiles.size());
    for (size_t i = 0; i < landscapeTiles.size(); ++i) pushed.push_back(false);

    do
    {
        allPushed = true;
        for (size_t i = 0; i < landscapeTiles.size(); ++i)
        {
            if (pushed[i]) continue;

            try
            {
                auto claim = cache->get(threadId, landscapeTiles[i]).claimable.claim(threadId, 0);
                claim.get().setYBounds(readback[i * 2], readback[i * 2 + 1]);
                pushed[i] = true;
            }
            catch (AFK_PolymerOutOfRange) { pushed[i] = true; /* Ignore, no entry any more */ }
            catch (AFK_ClaimException) { allPushed = false; /* Want to retry */ }
        }

        if (!allPushed) std::this_thread::yield();
    } while (!allPushed);
}

