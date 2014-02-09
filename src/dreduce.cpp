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
    computer(_computer)
{
    if (!computer->findKernel("makeShape3DVapourDReduce", dReduceKernel))
        throw AFK_Exception("Cannot find D-reduce kernel");
}

AFK_DReduce::~AFK_DReduce()
{
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
    auto kernelQueue = computer->getKernelQueue();

    kernelQueue->kernel(dReduceKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), units);
    kernelQueue->kernelArg(sizeof(cl_int2), &fake3D_size.v[0]);
    kernelQueue->kernelArg(sizeof(cl_int), &fake3D_mult);

    for (int vfI = 0; vfI < 4; ++vfI)
        kernelQueue->kernelArg(sizeof(cl_mem), &vapourJigsawsDensityMem[vfI]);

    kernelQueue->kernelArg(sizeof(cl_mem), densityRb.readyForKernel(computer, unitCount));
    kernelQueue->kernelArg(sizeof(cl_mem), colourRb.readyForKernel(computer, unitCount));

    size_t dReduceGlobalDim[3];
    dReduceGlobalDim[0] = unitCount;
    dReduceGlobalDim[1] = dReduceGlobalDim[2] = (1uLL << sSizes.getReduceOrder());

    size_t dReduceLocalDim[3];
    dReduceLocalDim[0] = 1;
    dReduceLocalDim[1] = dReduceLocalDim[2] = (1uLL << sSizes.getReduceOrder());

    kernelQueue->kernel3D(dReduceGlobalDim, dReduceLocalDim, preDep, o_postDep);

    densityRb.enqueueReadback(o_postDep);
    colourRb.enqueueReadback(o_postDep);
}

void AFK_DReduce::readBack(
    unsigned int threadId,
    const std::vector<AFK_KeyedCell>& shapeCells,
    AFK_SHAPE_CELL_CACHE *cache)
{
    densityRb.readbackFinish([threadId, shapeCells, cache](size_t i, const Vec2<float>& dMinMax)
    {
        auto entry = cache->get(threadId, shapeCells[i]);
        if (entry)
        {
            auto claim = entry->claimable.claim(threadId, 0);
            if (claim.isValid())
            {
                claim.get().setDMinMax(dMinMax.v[0], dMinMax.v[1]);
                return true;
            }
            else return false;
        }
        else return true; /* nothing to do */
    }, true);

    colourRb.readbackFinish([threadId, shapeCells, cache](size_t i, const Vec4<uint8_t>& colour)
    {
        auto entry = cache->get(threadId, shapeCells[i]);
        if (entry)
        {
            auto claim = entry->claimable.claim(threadId, 0);
            if (claim.isValid())
            {
                claim.get().setAvgColour(afk_vec3<float>(
                    static_cast<float>(colour.v[0]) / 256.0f,
                    static_cast<float>(colour.v[1]) / 256.0f,
                    static_cast<float>(colour.v[2]) / 256.0f));
                return true;
            }
            else return false;
        }
        else return true; /* nothing to do */
    }, true);
}

