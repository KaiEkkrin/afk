/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "3d_vapour_compute_queue.hpp"
#include "exception.hpp"


/* AFK_3DVapourComputeUnit implementation */

AFK_3DVapourComputeUnit::AFK_3DVapourComputeUnit():
    cubeOffset(-1)
{
}

AFK_3DVapourComputeUnit::AFK_3DVapourComputeUnit(
    const Vec4<float>& _location,
    const Vec4<float>& _baseColour,
    const AFK_JigsawPiece& _vapourJigsawPiece,
    int _adjacencies,
    int _cubeOffset,
    int _cubeCount):
        location(_location),
        baseColour(_baseColour),
        adjacencies(_adjacencies),
        cubeOffset(_cubeOffset),
        cubeCount(_cubeCount)
{
	vapourPiece = afk_vec4<int>(
		_vapourJigsawPiece.u,
		_vapourJigsawPiece.v,
		_vapourJigsawPiece.w,
		_vapourJigsawPiece.puzzle);
}

bool AFK_3DVapourComputeUnit::uninitialised(void) const
{
    return (cubeOffset < 0);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << unit.location;
    os << ", baseColour=" << std::dec << unit.baseColour;
    os << ", adjacencies=" << std::hex << unit.adjacencies;
    os << ", cubeOffset=" << std::dec << unit.cubeOffset;
    os << ", cubeCount=" << std::dec << unit.cubeCount;
    os << ", vapourPiece=" << std::dec << unit.vapourPiece;
    os << ")";
    return os;
}


/* AFK_3DVapourComputeQueue implementation */

AFK_3DVapourComputeQueue::AFK_3DVapourComputeQueue():
    vapourFeatureKernel(0),
    vapourNormalKernel(0)
{
}

AFK_3DVapourComputeQueue::~AFK_3DVapourComputeQueue()
{
}

void AFK_3DVapourComputeQueue::extend(
    const AFK_3DList& list,
    unsigned int& o_cubeOffset,
    unsigned int& o_cubeCount)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* Sanity checks */
    assert(list.cubeCount() > 0);

    o_cubeOffset = AFK_3DList::cubeCount();
    o_cubeCount = list.cubeCount();
    AFK_3DList::extend(list);
}

AFK_3DVapourComputeUnit AFK_3DVapourComputeQueue::addUnit(
    const Vec4<float>& location,
    const Vec4<float>& baseColour,
    const AFK_JigsawPiece& vapourJigsawPiece,
    int adjacencies,
    unsigned int cubeOffset,
    unsigned int cubeCount)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* Sanity checks */
    assert(cubeOffset >= 0 && cubeOffset < c.size());
    assert(cubeCount > 0);
    assert((cubeOffset + cubeCount) <= c.size());

    AFK_3DVapourComputeUnit newUnit(
        location,
        baseColour,
        vapourJigsawPiece,
        adjacencies,
        cubeOffset,
        cubeCount);
    units.push_back(newUnit);
    return newUnit;
}

void AFK_3DVapourComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_JigsawCollection *vapourJigsaws,
    const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!vapourFeatureKernel)
        if (!computer->findKernel("makeShape3DVapourFeature", vapourFeatureKernel))
            throw AFK_Exception("Cannot find 3D vapour feature kernel");

    if (!vapourNormalKernel)
        if (!computer->findKernel("makeShape3DVapourNormal", vapourNormalKernel))
            throw AFK_Exception("Cannot find 3D vapour normal kernel");

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the vapour inputs to CL buffers. */
    cl_mem vapourBufs[3];

    vapourBufs[0] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        f.size() * sizeof(AFK_3DVapourFeature),
        &f[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    vapourBufs[1] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        c.size() * sizeof(AFK_3DVapourCube),
        &c[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    vapourBufs[2] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_3DVapourComputeUnit),
        &units[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    /* Set up the rest of the vapour parameters */
    preVapourWaitList.clear();
    cl_mem *vapourJigsawsMem[4];
    int jpCount = vapourJigsaws->acquireAllForCl(ctxt, q, vapourJigsawsMem, 4, preVapourWaitList);

    AFK_CLCHK(clSetKernelArg(vapourFeatureKernel, 0, sizeof(cl_mem), &vapourBufs[0]))
    AFK_CLCHK(clSetKernelArg(vapourFeatureKernel, 1, sizeof(cl_mem), &vapourBufs[1]))
    AFK_CLCHK(clSetKernelArg(vapourFeatureKernel, 2, sizeof(cl_mem), &vapourBufs[2]))

    for (int jpI = 0; jpI < 4; ++jpI)
        AFK_CLCHK(clSetKernelArg(vapourFeatureKernel, jpI + 3, sizeof(cl_mem), &vapourJigsawsMem[jpI][0]))

    size_t vapourDim[3];
    vapourDim[0] = sSizes.tDim * unitCount;
    vapourDim[1] = vapourDim[2] = sSizes.tDim;

    cl_event vapourFeatureEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, vapourFeatureKernel, 3, NULL, &vapourDim[0], NULL,
        preVapourWaitList.size(),
        &preVapourWaitList[0],
        &vapourFeatureEvent))

    /* Release the things */
    for (std::vector<cl_event>::iterator evIt = preVapourWaitList.begin();
        evIt != preVapourWaitList.end(); ++evIt)
    {
        if (*evIt) AFK_CLCHK(clReleaseEvent(*evIt))
    }

    /* Next, compute the vapour normals. */
    AFK_CLCHK(clSetKernelArg(vapourNormalKernel, 0, sizeof(cl_mem), &vapourBufs[2]))
    for (int jpI = 0; jpI < 4; ++jpI)
    {
        AFK_CLCHK(clSetKernelArg(vapourNormalKernel, jpI + 1, sizeof(cl_mem), &vapourJigsawsMem[jpI][0])) /* feature */
        AFK_CLCHK(clSetKernelArg(vapourNormalKernel, jpI + 5, sizeof(cl_mem), &vapourJigsawsMem[jpI][1])) /* normal */
    }

    cl_event vapourNormalEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, vapourNormalKernel, 3, NULL, &vapourDim[0], NULL,
        1, &vapourFeatureEvent, &vapourNormalEvent))

    for (unsigned int i = 0; i < 3; ++i)
    {
        AFK_CLCHK(clReleaseMemObject(vapourBufs[i]))
    }

    postVapourWaitList.clear();
    postVapourWaitList.push_back(vapourNormalEvent);
    vapourJigsaws->releaseAllFromCl(q, vapourJigsawsMem, jpCount, postVapourWaitList);
    if (vapourFeatureEvent) AFK_CLCHK(clReleaseEvent(vapourFeatureEvent))
    if (vapourNormalEvent) AFK_CLCHK(clReleaseEvent(vapourNormalEvent))

    computer->unlock();
}

void AFK_3DVapourComputeQueue::computeFinish(void)
{
    /* This method is included for symmetry with TerrainComputeQueue.
     * Right now it does nothing.
     */
}

bool AFK_3DVapourComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
}

void AFK_3DVapourComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    f.clear();
    c.clear();
    units.clear();
}

