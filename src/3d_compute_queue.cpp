/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "3d_compute_queue.hpp"
#include "exception.hpp"


/* AFK_3DComputeUnit implementation */

AFK_3DComputeUnit::AFK_3DComputeUnit():
    cubeOffset(-1)
{
}

AFK_3DComputeUnit::AFK_3DComputeUnit(
    const Vec4<float>& _location,
    const Vec3<int>& _vapourPiece,
    const Vec2<int>& _edgePiece,
    int _cubeOffset,
    int _cubeCount):
        location(_location),
        vapourPiece(_vapourPiece),
        edgePiece(_edgePiece),
        cubeOffset(_cubeOffset),
        cubeCount(_cubeCount)
{
}

bool AFK_3DComputeUnit::uninitialised(void) const
{
    return (cubeOffset < 0);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << unit.location;
    os << ", cubeOffset=" << std::dec << unit.cubeOffset;
    os << ", cubeCount=" << std::dec << unit.cubeCount;
    os << ", vapourPiece=" << std::dec << unit.vapourPiece;
    os << ", edgePiece=" << std::dec << unit.edgePiece;
    os << ")";
    return os;
}


/* AFK_3DComputeQueue implementation */

AFK_3DComputeQueue::AFK_3DComputeQueue():
    vapourKernel(0), edgeKernel(0)
{
}

AFK_3DComputeQueue::~AFK_3DComputeQueue()
{
}

AFK_3DComputeUnit AFK_3DComputeQueue::extend(
    const AFK_3DList& list,
    const AFK_ShapeCube& shapeCube,
    const AFK_ShapeSizes & sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* Sanity checks */
    if (list.cubeCount() == 0)
    {
        std::ostringstream ss;
        ss << "Pushed empty list to 3D compute queue for piece " << std::dec << face.jigsawPiece;
        throw AFK_Exception(ss.str());
    }

    if (list.pointCount() != (list.cubeCount() * sSizes.pointCountPerCube))
        throw AFK_Exception("Insane self found");

    AFK_3DComputeUnit newUnit(
        shapeCube.location,
        shapeCube.vapourJigsawPiece.piece,
        shapeCube.edgeJigsawPiece.piece,
        AFK_3DList::cubeCount(),
        list.cubeCount());
    AFK_3DList::extend(list);
    units.push_back(newUnit);
    return newUnit;
}

AFK_3DComputeUnit AFK_3DComputeQueue::addUnitWithExisting(
    const AFK_3DComputeUnit& existingUnit,
    const AFK_ShapeCube& shapeCube)
{
    boost::unique_lock<boost::mutex> lock(mut);

    if (existingUnit.uninitialised()) throw AFK_Exception("Tried to add unit from an uninitialised existing one");

    AFK_3DComputeUnit newUnit(
        shapeCube.location,
        shapeCube.vapourJigsawPiece.piece,
        shapeCube.edgeJigsawPiece.piece,
        existingUnit.cubeOffset,
        existingUnit.cubeCount);
    units.push_back(newUnit);
    return newUnit;
}

void AFK_3DComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_Jigsaw *vapourJigsaw,
    AFK_Jigsaw *edgeJigsaw,
    const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!vapourKernel)
        if (!computer->findKernel("makeShape3Dvapour", vapourKernel))
            throw AFK_Exception("Cannot find 3D vapour kernel");

    if (!edgeKernel)
        if (!computer->findKernel("makeShape3Dedge", edgeKernel))
            throw AFK_Exception("Cannot find 3D edge kernel");

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the vapour inputs to CL buffers. */
    cl_mem vapourBufs[3];

    vapourBufs[0] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        p.size() * sizeof(AFK_3DPoint),
        &p[0], &error);
    afk_handleClError(error);

    vapourBufs[1] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        c.size() * sizeof(AFK_3DCube),
        &c[0], &error);
    afk_handleClError(error);

    vapourBufs[2] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_3DComputeUnit),
        &units[0], &error);
    afk_handleClError(error);

    /* Set up the rest of the vapour parameters */
    cl_event acquireVapourJigsawEvent = 0;
    cl_mem *vapourJigsawMem = vapourJigsaw->acquireForCl(ctxt, q, &acquireVapourJigsawEvent);

    AFK_CLCHK(clSetKernelArg(vapourKernel, 0, sizeof(cl_mem), &vapourBufs[0]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 1, sizeof(cl_mem), &vapourBufs[1]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 2, sizeof(cl_mem), &vapourBufs[2]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 3, sizeof(cl_mem), &vapourJigsawMem[0]))

    size_t vapourDim[3];
    vapourDim[0] = sSizes.tDim * unitCount;
    vapourDim[1] = vapourDim[2] = sSizes.tDim;

    /* For the edge kernel, I'm going to need the edge
     * jigsaw to have been acquired, and also the vapour
     * kernel to have completed.
     */
    cl_event vapourEvents[2];

    AFK_CLCHK(clEnqueueNDRangeKernel(q, shrinkformKernel, 3, NULL, &shrinkformDim[0], NULL,
        acquireJigsawEvent ? 1 : 0,
        acquireJigsawEvent ? &acquireJigsawEvent : NULL,
        &vapourEvents[0]))

    if (acquireJigsawEvent) AFK_CLCHK(clReleaseEvent(acquireJigsawEvent))

    vapourEvents[1] = 0;
    cl_mem *edgeJigsawMem = edgeJigsaw->acquireForCl(ctxt, q, &vapourEvents[1]);

    /* Now I'm also going to need this... */
    cl_sampler vapourJigsawSampler = clCreateSampler(
        ctxt,
        CL_FALSE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_FILTER_NEAREST,
        &error);
    afk_handleClError(error);

    /* Now, I need to run the edge kernel.
     */
    AFK_CLCHK(clSetKernelArg(edgeKernel, 0, sizeof(cl_mem), &vapourJigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 1, sizeof(cl_sampler), &vapourJigsawSampler))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 2, sizeof(cl_mem), &vapourBufs[2]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 3, sizeof(cl_mem), &edgeJigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 4, sizeof(cl_mem), &edgeJigsawMem[1]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 5, sizeof(cl_mem), &edgeJigsawMem[2]))

    size_t edgeGlobalDim[3];
    edgeGlobalDim[0] = (sSizes.tDim - 1) * unitCount;
    edgeGlobalDim[1] = edgeGlobalDim[2] = (sSizes.tDim - 1);

    size_t edgeLocalDim[3];
    edgeLocalDim[0] = edgeLocalDim[1] = edgeLocalDim[2] =
        (sSizes.tDim - 1);

    cl_event edgeEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, surfaceKernel, 3, 0, &surfaceGlobalDim[0], &surfaceLocalDim[0],
        vapourEvents[1] != 0 ? 2 : 1,
        &vapourEvents[0],
        &edgeEvent))

    /* Release the things */
    AFK_CLCHK(clReleaseSampler(vapourSampler))
    AFK_CLCHK(clReleaseEvent(vapourEvents[0]))
    if (vapourEvents[1] != 0) AFK_CLCHK(clReleaseEvent(vapourEvents[1]))

    for (unsigned int i = 0; i < 3; ++i)
    {
        AFK_CLCHK(clReleaseMemObject(vapourBufs[i]))
    }

    vapourJigsaw->releaseFromCl(q, 1, &edgeEvent);
    edgeJigsaw->releaseFromCl(q, 1, &edgeEvent);
    AFK_CLCHK(clReleaseEvent(edgeEvent))

    computer->unlock();
}

void AFK_3DComputeQueue::computeFinish(void)
{
    /* This method is included for symmetry with TerrainComputeQueue.
     * Right now it does nothing.
     */
}

bool AFK_3DComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
}

void AFK_3DComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    p.clear();
    c.clear();
    units.clear();
}

