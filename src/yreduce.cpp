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

#include "core.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
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
    if (readback != nullptr) delete[] readback;
}

void AFK_YReduce::compute(
    unsigned int unitCount,
    cl_mem *units,
    cl_mem *jigsawYDisp,
    cl_sampler *yDispSampler,
    const AFK_LandscapeSizes& lSizes,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    cl_int error;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

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

    AFK_CLCHK(computer->oclShim.SetKernelArg()(yReduceKernel, 0, sizeof(cl_mem), units))
    AFK_CLCHK(computer->oclShim.SetKernelArg()(yReduceKernel, 1, sizeof(cl_mem), jigsawYDisp))
    AFK_CLCHK(computer->oclShim.SetKernelArg()(yReduceKernel, 2, sizeof(cl_sampler), yDispSampler))
    AFK_CLCHK(computer->oclShim.SetKernelArg()(yReduceKernel, 3, sizeof(cl_mem), &buf))

    size_t yReduceGlobalDim[2];
    yReduceGlobalDim[0] = (1 << lSizes.getReduceOrder());
    yReduceGlobalDim[1] = unitCount;

    size_t yReduceLocalDim[2];
    yReduceLocalDim[0] = (1 << lSizes.getReduceOrder());
    yReduceLocalDim[1] = 1;

    AFK_CLCHK(computer->oclShim.EnqueueNDRangeKernel()(q, yReduceKernel, 2, 0, &yReduceGlobalDim[0], &yReduceLocalDim[0],
        preDep.getEventCount(), preDep.getEvents(), o_postDep.addEvent()))

    size_t requiredReadbackSize = requiredSize / sizeof(float);
    if (readbackSize < requiredReadbackSize)
    {
        if (readback) delete[] readback;
        readback = new float[requiredReadbackSize];
        readbackSize = requiredReadbackSize;
    }

    /* TODO If I make this asynchronous (change CL_TRUE to CL_FALSE), I get
     * kersplode on AMD.
     * I think what's going on is the OpenGL memory management gets
     * confused in the presence of buffers; try making the readback
     * an image instead and see if it's happier.
     */
    AFK_CLCHK(computer->oclShim.EnqueueReadBuffer()(q, buf,
        afk_core.computer->isAMD() ? CL_TRUE : CL_FALSE,
        0, requiredSize, readback, o_postDep.getEventCount(), o_postDep.getEvents(), readbackDep.addEvent()))

    computer->unlock();
}

void AFK_YReduce::readBack(
    unsigned int unitCount,
    std::vector<AFK_LandscapeTile*>& landscapeTiles)
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

    for (unsigned int i = 0; i < unitCount; ++i)
    {
        landscapeTiles[i]->setYBounds(
            readback[i * 2], readback[i * 2 + 1]);
    }
}

