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

#include <thread>

#include "core.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "world.hpp"
#include "yreduce.hpp"

AFK_YReduce::AFK_YReduce(AFK_Computer *_computer):
    computer(_computer)
{
    if (!computer->findKernel("makeLandscapeYReduce", yReduceKernel))
        throw AFK_Exception("Cannot find Y-reduce kernel");
}

AFK_YReduce::~AFK_YReduce()
{
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
    auto kernelQueue = computer->getKernelQueue();

    kernelQueue->kernel(yReduceKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), units);
    kernelQueue->kernelArg(sizeof(cl_mem), jigsawYDisp);
    kernelQueue->kernelArg(sizeof(cl_sampler), yDispSampler);
    kernelQueue->kernelArg(sizeof(cl_mem), yRb.readyForKernel(computer, unitCount));

    size_t yReduceGlobalDim[2];
    yReduceGlobalDim[0] = (1uLL << lSizes.getReduceOrder());
    yReduceGlobalDim[1] = unitCount;

    size_t yReduceLocalDim[2];
    yReduceLocalDim[0] = (1uLL << lSizes.getReduceOrder());
    yReduceLocalDim[1] = 1;

    kernelQueue->kernel2D(yReduceGlobalDim, yReduceLocalDim, preDep, o_postDep);

    yRb.enqueueReadback(o_postDep);
}

void AFK_YReduce::readBack(
    unsigned int threadId,
    const std::vector<AFK_Tile>& landscapeTiles,
    AFK_LANDSCAPE_CACHE *cache)
{
    yRb.readbackFinish([threadId, landscapeTiles, cache](size_t i, const Vec2<float>& yBounds)
    {
        auto entry = cache->get(threadId, landscapeTiles[i]);
        if (entry)
        {
            auto claim = entry->claimable.claim(threadId, 0);
            if (claim.isValid())
            {
                claim.get().setYBounds(yBounds.v[0], yBounds.v[1]);
                return true;
            }
            else return false;
        }
        else return true; /* nothing to do */
    }, true);
}
