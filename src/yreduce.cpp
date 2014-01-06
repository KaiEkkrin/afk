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
    computer(_computer), deviceBuf(0), hostBuf(0), bufSize(0), readback(nullptr),
    readbackMappedDep(_computer), readbackUnmappedDep(_computer)
{
    if (!computer->findKernel("makeLandscapeYReduce", yReduceKernel))
        throw AFK_Exception("Cannot find Y-reduce kernel");
}

AFK_YReduce::~AFK_YReduce()
{
    if (deviceBuf) computer->oclShim.ReleaseMemObject()(deviceBuf);
    if (hostBuf) computer->oclShim.ReleaseMemObject()(hostBuf);
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
    auto hostQueue = computer->getHostQueue();

    /* The buffer needs to be big enough for 2 floats per
     * unit.
     */
    size_t requiredSize = unitCount * 2 * sizeof(float);
    if (bufSize < requiredSize)
    {
        if (deviceBuf) AFK_CLCHK(computer->oclShim.ReleaseMemObject()(deviceBuf))
        if (hostBuf) AFK_CLCHK(computer->oclShim.ReleaseMemObject()(hostBuf))

        deviceBuf = computer->oclShim.CreateBuffer()(
            ctxt,
            CL_MEM_WRITE_ONLY,
            requiredSize,
            nullptr,
            &error);
        AFK_HANDLE_CL_ERROR(error);

        hostBuf = computer->oclShim.CreateBuffer()(
            ctxt,
            CL_MEM_ALLOC_HOST_PTR,
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
    kernelQueue->kernelArg(sizeof(cl_mem), &deviceBuf);

    size_t yReduceGlobalDim[2];
    yReduceGlobalDim[0] = (1uLL << lSizes.getReduceOrder());
    yReduceGlobalDim[1] = unitCount;

    size_t yReduceLocalDim[2];
    yReduceLocalDim[0] = (1uLL << lSizes.getReduceOrder());
    yReduceLocalDim[1] = 1;

    kernelQueue->kernel2D(yReduceGlobalDim, yReduceLocalDim, preDep, o_postDep);

    /* After the kernel has executed, enqueue a copy-back-and-map right away,
     * so that readBack can wait for the map and have things ready.
     * To be pedantic, I shouldn't copy until any leftover mapping is over:
     */
    AFK_ComputeDependency readbackCopiedDep(computer);
    readbackUnmappedDep += o_postDep;
    readQueue->copyBuffer(deviceBuf, hostBuf, requiredSize, readbackUnmappedDep, readbackCopiedDep);
    readback = reinterpret_cast<float *>(
        hostQueue->mapBuffer(hostBuf, CL_MAP_READ, requiredSize, readbackCopiedDep, readbackMappedDep));
    readbackSize = requiredSize / sizeof(float);
}

// TODO: Do the readback call as a clSetEventCallback() -- neater.
void AFK_YReduce::readBack(
    unsigned int threadId,
    size_t unitCount,
    const std::vector<AFK_Tile>& landscapeTiles,
    AFK_LANDSCAPE_CACHE *cache)
{
    readbackMappedDep.waitFor();
    readbackMappedDep.reset();
    readbackUnmappedDep.reset();

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
    std::vector<bool> pushed(landscapeTiles.size(), false);

    do
    {
        allPushed = true;
        for (size_t i = 0; i < landscapeTiles.size(); ++i)
        {
            if (pushed[i]) continue;

            auto entry = cache->get(threadId, landscapeTiles[i]);
            if (entry)
            {
                auto claim = entry->claimable.claim(threadId, 0);
                if (claim.isValid())
                {
                    claim.get().setYBounds(readback[i * 2], readback[i * 2 + 1]);
                    pushed[i] = true;
                }
                else
                {
                    /* want to retry */
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

    /* And after all that, unmap the readback ready for the next pass */
    auto hostQueue = computer->getHostQueue();
    hostQueue->unmapObject(hostBuf, readback, readbackMappedDep, readbackUnmappedDep);
    readback = nullptr;
    readbackSize = 0;
}
