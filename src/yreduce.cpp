/* AFK (c) Alex Holloway 2013 */

#include "exception.hpp"
#include "landscape_tile.hpp"
#include "yreduce.hpp"

AFK_YReduce::AFK_YReduce(const AFK_Computer *computer):
    buf(0), bufSize(0), readback(NULL), readbackSize(0), readbackEvent(0)
{
    if (!computer->findKernel("yReduce", yReduceKernel))
        throw AFK_Exception("Cannot find Y-reduce kernel");
}

AFK_YReduce::~AFK_YReduce()
{
    if (buf) clReleaseMemObject(buf);
    if (readbackEvent) clReleaseEvent(readbackEvent);
    if (readback != NULL) delete[] readback;
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
    const cl_event *eventWaitList)
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
            NULL,
            &error);
        afk_handleClError(error);
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

    cl_event yReduceEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, yReduceKernel, 2, 0, &yReduceGlobalDim[0], &yReduceLocalDim[0],
        eventsInWaitList, eventWaitList, &yReduceEvent))

    size_t requiredReadbackSize = bufSize / sizeof(float);
    if (readbackSize < requiredReadbackSize)
    {
        if (readback) delete[] readback;
        readback = new float[requiredReadbackSize];
        readbackSize = requiredReadbackSize;
    }

    AFK_CLCHK(clEnqueueReadBuffer(q, buf, CL_FALSE, 0, bufSize, readback, 1, &yReduceEvent, &readbackEvent))
    AFK_CLCHK(clReleaseEvent(yReduceEvent))
}

void AFK_YReduce::readBack(
    unsigned int unitCount,
    std::vector<AFK_LandscapeTile*>& landscapeTiles)
{
    if (!readbackEvent) return;

    clWaitForEvents(1, &readbackEvent);
    clReleaseEvent(readbackEvent);
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

