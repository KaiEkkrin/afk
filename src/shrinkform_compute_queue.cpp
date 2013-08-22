/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "exception.hpp"
#include "shrinkform_compute_queue.hpp"


/* AFK_ShrinkformComputeUnit implementation */

AFK_ShrinkformComputeUnit::AFK_ShrinkformComputeUnit(
    int _cubeOffset,
    int _cubeCount,
    const Vec2<int>& _piece):
        cubeOffset(_cubeOffset),
        cubeCount(_cubeCount),
        piece(_piece)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformComputeUnit& unit)
{
    os << "(SCU: ";
    os << "cubeOffset=" << std::dec << unit.cubeOffset;
    os << ", cubeCount=" << std::dec << unit.cubeCount;
    os << ", piece=" << std::dec << unit.piece;
    os << ")";
    return os;
}


/* AFK_ShrinkformComputeQueue implementation */

AFK_ShrinkformComputeQueue::AFK_ShrinkformComputeQueue():
    shrinkformKernel(0), surfaceKernel(0)
{
}

AFK_ShrinkformComputeQueue::~AFK_ShrinkformComputeQueue()
{
}

AFK_ShrinkformComputeUnit AFK_ShrinkformComputeQueue::extend(const AFK_ShrinkformList& list, const Vec2<int>& piece, const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* Sanity checks */
    if (list.cubeCount() == 0)
    {
        std::ostringstream ss;
        ss << "Pushed empty list to shrinkform compute queue for piece " << std::dec << piece;
        throw AFK_Exception(ss.str());
    }

    if (list.pointCount() != (list.cubeCount() * sSizes.pointCountPerCube))
        throw AFK_Exception("Insane self found");

    AFK_ShrinkformComputeUnit newUnit(
        AFK_ShrinkformList::cubeCount(),
        list.cubeCount(),
        piece);
    AFK_ShrinkformList::extend(list);
    units.push_back(newUnit);
    return newUnit;
}

void AFK_ShrinkformComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_Jigsaw *jigsaw,
    const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!shrinkformKernel)
        if (!computer->findKernel("makeShrinkform", shrinkformKernel))
            throw AFK_Exception("Cannot find shrinkform kernel");

    if (!surfaceKernel)
        if (!computer->findKernel("makeSurface", surfaceKernel))
            throw AFK_Exception("Cannot find surface kernel");

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the shrinkform inputs to CL buffers. */
    cl_mem shrinkformBufs[3];

    shrinkformBufs[0] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        p.size() * sizeof(AFK_ShrinkformPoint),
        &p[0], &error);
    afk_handleClError(error);

    shrinkformBufs[1] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        c.size() * sizeof(AFK_ShrinkformCube),
        &c[0], &error);
    afk_handleClError(error);

    shrinkformBufs[2] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_ShrinkformComputeUnit),
        &units[0], &error);
    afk_handleClError(error);

    /* Set up the rest of the shrinkform parameters */
    cl_event acquireJigsawEvent = 0;
    cl_mem *jigsawMem = jigsaw->acquireForCl(ctxt, q, &acquireJigsawEvent);

    AFK_CLCHK(clSetKernelArg(shrinkformKernel, 0, sizeof(cl_mem), &shrinkformBufs[0]))
    AFK_CLCHK(clSetKernelArg(shrinkformKernel, 1, sizeof(cl_mem), &shrinkformBufs[1]))
    AFK_CLCHK(clSetKernelArg(shrinkformKernel, 2, sizeof(cl_mem), &shrinkformBufs[2]))
    AFK_CLCHK(clSetKernelArg(shrinkformKernel, 3, sizeof(cl_mem), &jigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(shrinkformKernel, 4, sizeof(cl_mem), &jigsawMem[1]))

    size_t shrinkformDim[3];
    shrinkformDim[0] = shrinkformDim[1] = sSizes.tDim;
    shrinkformDim[2] = unitCount;

    cl_event shrinkformEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, shrinkformKernel, 3, NULL, &shrinkformDim[0], NULL,
        acquireJigsawEvent ? 1 : 0,
        acquireJigsawEvent ? &acquireJigsawEvent : NULL,
        &shrinkformEvent))

    if (acquireJigsawEvent) AFK_CLCHK(clReleaseEvent(acquireJigsawEvent))

    /* Now I'm going to need this... */
    cl_sampler jigsawDispSampler = clCreateSampler(
        ctxt,
        CL_FALSE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_FILTER_NEAREST,
        &error);
    afk_handleClError(error);

    /* Now, I need to run the kernel to bake the surface normals.
     */
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 0, sizeof(cl_mem), &shrinkformBufs[2]))
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 1, sizeof(cl_mem), &jigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 2, sizeof(cl_sampler), &jigsawDispSampler))
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 3, sizeof(cl_mem), &jigsawMem[2]))

    size_t surfaceGlobalDim[3];
    surfaceGlobalDim[0] = surfaceGlobalDim[1] = sSizes.tDim - 1;
    surfaceGlobalDim[2] = unitCount;

    size_t surfaceLocalDim[3];
    surfaceLocalDim[0] = surfaceLocalDim[1] = sSizes.tDim - 1;
    surfaceLocalDim[2] = 1;

    cl_event surfaceEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, surfaceKernel, 3, 0, &surfaceGlobalDim[0], &surfaceLocalDim[0],
        1, &shrinkformEvent, &surfaceEvent))

    /* Release the things */
    AFK_CLCHK(clReleaseSampler(jigsawDispSampler))
    AFK_CLCHK(clReleaseEvent(shrinkformEvent))

    for (unsigned int i = 0; i < 3; ++i)
    {
        AFK_CLCHK(clReleaseMemObject(shrinkformBufs[i]))
    }

    jigsaw->releaseFromCl(q, 1, &surfaceEvent);
    AFK_CLCHK(clReleaseEvent(surfaceEvent))

    computer->unlock();
}

void AFK_ShrinkformComputeQueue::computeFinish(void)
{
    /* This method is included for symmetry with TerrainComputeQueue.
     * Right now it does nothing.
     */
}

bool AFK_ShrinkformComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
}

void AFK_ShrinkformComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    p.clear();
    c.clear();
    units.clear();
}

