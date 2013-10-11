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

AFK_YReduce::AFK_YReduce(const AFK_Computer *computer):
    buf(0), bufSize(0), readback(nullptr), readbackSize(0), readbackEvent(0)
{
    if (!computer->findKernel("makeLandscapeYReduce", yReduceKernel))
        throw AFK_Exception("Cannot find Y-reduce kernel");
}

AFK_YReduce::~AFK_YReduce()
{
    if (buf) clReleaseMemObject(buf);
    if (readbackEvent) clReleaseEvent(readbackEvent);
    if (readback != nullptr) delete[] readback;
}

void AFK_YReduce::compute(
    cl_context ctxt,
    cl_command_queue q,
    unsigned int unitCount,
    cl_mem *units,
    cl_mem *jigsawYDisp,
    cl_sampler *yDispSampler,
    const AFK_LandscapeSizes& lSizes,
    cl_uint eventsInWaitList,
    const cl_event *eventWaitList,
    cl_event *o_event)
{
    cl_int error;

    /* The buffer needs to be big enough for 2 floats per
     * unit.
     */
    size_t requiredSize = unitCount * 2 * sizeof(float);
    if (bufSize < requiredSize)
    {
        if (buf)
            AFK_CLCHK(clReleaseMemObject(buf))

        buf = clCreateBuffer(
            ctxt, CL_MEM_WRITE_ONLY,
            requiredSize,
            nullptr,
            &error);
        AFK_HANDLE_CL_ERROR(error);
        bufSize = requiredSize;
    }

    AFK_CLCHK(clSetKernelArg(yReduceKernel, 0, sizeof(cl_mem), units))
    AFK_CLCHK(clSetKernelArg(yReduceKernel, 1, sizeof(cl_mem), jigsawYDisp))
    AFK_CLCHK(clSetKernelArg(yReduceKernel, 2, sizeof(cl_sampler), yDispSampler))
    AFK_CLCHK(clSetKernelArg(yReduceKernel, 3, sizeof(cl_mem), &buf))

    size_t yReduceGlobalDim[2];
    yReduceGlobalDim[0] = (1 << lSizes.getReduceOrder());
    yReduceGlobalDim[1] = unitCount;

    size_t yReduceLocalDim[2];
    yReduceLocalDim[0] = (1 << lSizes.getReduceOrder());
    yReduceLocalDim[1] = 1;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, yReduceKernel, 2, 0, &yReduceGlobalDim[0], &yReduceLocalDim[0],
        eventsInWaitList, eventWaitList, o_event))

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
    AFK_CLCHK(clEnqueueReadBuffer(q, buf,
        afk_core.computer->isAMD() ? CL_TRUE : CL_FALSE,
        0, requiredSize, readback, 1, o_event, &readbackEvent))
}

void AFK_YReduce::readBack(
    unsigned int unitCount,
    std::vector<AFK_LandscapeTile*>& landscapeTiles)
{
    if (!readbackEvent) return;

    AFK_CLCHK(clWaitForEvents(1, &readbackEvent))
    AFK_CLCHK(clReleaseEvent(readbackEvent))
    readbackEvent = 0;

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

