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
    const AFK_JigsawPiece& _vapourJigsawPiece,
    int _cubeOffset,
    int _cubeCount):
        location(_location),
        cubeOffset(_cubeOffset),
        cubeCount(_cubeCount)
{
	vapourPiece = afk_vec4<int>(
		_vapourJigsawPiece.u,
		_vapourJigsawPiece.v,
		_vapourJigsawPiece.w,
		0);
}

bool AFK_3DVapourComputeUnit::uninitialised(void) const
{
    return (cubeOffset < 0);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << unit.location;
    os << ", cubeOffset=" << std::dec << unit.cubeOffset;
    os << ", cubeCount=" << std::dec << unit.cubeCount;
    os << ", vapourPiece=" << std::dec << unit.vapourPiece;
    os << ")";
    return os;
}


/* AFK_3DVapourComputeQueue implementation */

AFK_3DVapourComputeQueue::AFK_3DVapourComputeQueue():
    vapourKernel(0)
{
}

AFK_3DVapourComputeQueue::~AFK_3DVapourComputeQueue()
{
}

AFK_3DVapourComputeUnit AFK_3DVapourComputeQueue::extend(
    const AFK_3DList& list,
    const Vec4<float>& location,
    const AFK_JigsawPiece& vapourJigsawPiece,
    const AFK_ShapeSizes & sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* Sanity checks */
    if (list.cubeCount() == 0)
    {
        std::ostringstream ss;
        ss << "Pushed empty list to 3D vapour compute queue for piece at " << location;
        throw AFK_Exception(ss.str());
    }

    if (list.featureCount() != (list.cubeCount() * sSizes.featureCountPerCube))
        throw AFK_Exception("Insane self found");

    AFK_3DVapourComputeUnit newUnit(
        location,
        vapourJigsawPiece,
        AFK_3DList::cubeCount(),
        list.cubeCount());
    AFK_3DList::extend(list);
    units.push_back(newUnit);
    return newUnit;
}

AFK_3DVapourComputeUnit AFK_3DVapourComputeQueue::addUnitFromExisting(
    const AFK_3DVapourComputeUnit& existingUnit,
    const Vec4<float>& location,
    const AFK_JigsawPiece& vapourJigsawPiece)
{
    boost::unique_lock<boost::mutex> lock(mut);

    if (existingUnit.uninitialised()) throw AFK_Exception("Tried to add unit from an uninitialised existing one");

    AFK_3DVapourComputeUnit newUnit(
        location,
        vapourJigsawPiece,
        existingUnit.cubeOffset,
        existingUnit.cubeCount);
    units.push_back(newUnit);
    return newUnit;
}

void AFK_3DVapourComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_Jigsaw *vapourJigsaw,
    const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!vapourKernel)
        if (!computer->findKernel("makeShape3DVapour", vapourKernel))
            throw AFK_Exception("Cannot find 3D vapour kernel");

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the vapour inputs to CL buffers. */
    cl_mem vapourBufs[3];

    vapourBufs[0] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        f.size() * sizeof(AFK_3DVapourFeature),
        &f[0], &error);
    afk_handleClError(error);

    vapourBufs[1] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        c.size() * sizeof(AFK_3DVapourCube),
        &c[0], &error);
    afk_handleClError(error);

    vapourBufs[2] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_3DVapourComputeUnit),
        &units[0], &error);
    afk_handleClError(error);

    /* TODO Remove Spam */
    for (std::vector<AFK_3DVapourComputeUnit>::iterator cuIt = units.begin();
        cuIt != units.end(); ++cuIt)
    {
        std::cout << "Using 3D vapour compute unit: " << *cuIt << std::endl;
    }

    /* Set up the rest of the vapour parameters */
    preVapourWaitList.clear();
    cl_mem *vapourJigsawMem = vapourJigsaw->acquireForCl(ctxt, q, preVapourWaitList);

    AFK_CLCHK(clSetKernelArg(vapourKernel, 0, sizeof(cl_mem), &vapourBufs[0]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 1, sizeof(cl_mem), &vapourBufs[1]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 2, sizeof(cl_mem), &vapourBufs[2]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 3, sizeof(cl_mem), &vapourJigsawMem[0]))

    size_t vapourDim[3];
    vapourDim[0] = sSizes.vDim * unitCount;
    vapourDim[1] = vapourDim[2] = sSizes.vDim;

    cl_event vapourEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, vapourKernel, 3, NULL, &vapourDim[0], NULL,
        preVapourWaitList.size(),
        &preVapourWaitList[0],
        &vapourEvent))

    /* Release the things */
    for (std::vector<cl_event>::iterator evIt = preVapourWaitList.begin();
        evIt != preVapourWaitList.end(); ++evIt)
    {
        AFK_CLCHK(clReleaseEvent(*evIt))
    }

    for (unsigned int i = 0; i < 3; ++i)
    {
        AFK_CLCHK(clReleaseMemObject(vapourBufs[i]))
    }

    postVapourWaitList.clear();
    postVapourWaitList.push_back(vapourEvent);
    vapourJigsaw->releaseFromCl(q, postVapourWaitList);
    AFK_CLCHK(clReleaseEvent(vapourEvent))

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
