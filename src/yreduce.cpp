/* AFK (c) Alex Holloway 2013 */

#include "exception.hpp"
#include "yreduce.hpp"

AFK_YReduce::AFK_YReduce(const AFK_Computer *computer):
    readback(NULL), readbackSize(0)
{
    if (!computer->findKernel("yReduce", yReduceKernel))
        throw AFK_Exception("Cannot find Y-reduce kernel");
}

AFK_YReduce::~AFK_YReduce()
{
    for (std::vector<cl_mem>::iterator bufIt = bufs.begin();
        bufIt != bufs.end(); ++bufIt)
    {
        if (*bufIt) AFK_CLCHK(clReleaseMemObject(*bufIt))
    }

    if (readback != NULL) delete[] readback;
}

void AFK_YReduce::compute(
    cl_context ctxt,
    cl_command_queue q,
    unsigned int puzzle,
    unsigned int unitCount,
    cl_mem *units,
    cl_mem *jigsawYDisp,
    cl_sampler *yDispSampler,
    AFK_LandscapeSizes& lSizes)
{
    cl_int error;

    while (bufs.size() <= puzzle)
    {
        bufs.push_back(0);
        bufSizes.push_back(0);
    }

    /* Each buffer needs to be big enough for 2 floats per
     * unit.
     */
    size_t requiredSize = unitCount * 2 * sizeof(float);
    if (bufSizes[puzzle] < requiredSize)
    {
        if (bufs[puzzle])
            AFK_CLCHK(clReleaseMemObject(bufs[puzzle]))

        bufs[puzzle] = clCreateBuffer(
            ctxt, CL_MEM_READ_WRITE,
            requiredSize,
            NULL,
            &error);
        afk_handleClError(error);
        bufSizes[puzzle] = requiredSize;
    }

    AFK_CLCHK(clSetKernelArg(yReduceKernel, 0, sizeof(cl_mem), units))
    AFK_CLCHK(clSetKernelArg(yReduceKernel, 1, sizeof(cl_mem), jigsawYDisp))
    AFK_CLCHK(clSetKernelArg(yReduceKernel, 2, sizeof(cl_sampler), yDispSampler))
    AFK_CLCHK(clSetKernelArg(yReduceKernel, 3, sizeof(cl_mem), &bufs[puzzle]))

    size_t yReduceGlobalDim[2];
    yReduceGlobalDim[0] = (1 << lSizes.getReduceOrder());
    yReduceGlobalDim[1] = unitCount;

    size_t yReduceLocalDim[2];
    yReduceLocalDim[0] = (1 << lSizes.getReduceOrder());
    yReduceLocalDim[1] = 1;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, yReduceKernel, 2, 0, &yReduceGlobalDim[0], &yReduceLocalDim[0], 0, NULL, NULL))

    /* TODO I'm sure there's a better (less synchronous) way to
     * do this, but it's complicated and when I've only got
     * one puzzle (the typical case) benefits will be marginal.
     */
    size_t requiredReadbackSize = bufSizes[puzzle] / sizeof(float);
    if (readbackSize < requiredReadbackSize)
    {
        if (readback) delete[] readback;
        readback = new float[requiredReadbackSize];
        readbackSize = requiredReadbackSize;
    }

    AFK_CLCHK(clEnqueueReadBuffer(q, bufs[puzzle], CL_TRUE, 0, bufSizes[puzzle], readback, 0, NULL, NULL))

    /* TODO do something proper with the y bounds.  For now
     * I'll just print them to make sure they're coming out
     * looking sensible.
     */
    std::cout << "Computed y bounds: ";
    for (unsigned int i = 0; i < 4 && i < unitCount; ++i)
    {
        if (i > 0) std::cout << ", ";
        std::cout << "(" << readback[i * 2] << ", " << readback[i * 2 + 1] << ")";
    }
    std::cout << std::endl;
}

