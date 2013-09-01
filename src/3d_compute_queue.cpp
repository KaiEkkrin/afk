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
    const AFK_JigsawPiece& _vapourJigsawPiece,
    const AFK_JigsawPiece& _edgeJigsawPiece,
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

	edgePiece = afk_vec2<int>(
		_edgeJigsawPiece.u,
		_edgeJigsawPiece.v);
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
        ss << "Pushed empty list to 3D compute queue for piece " << shapeCube;
        throw AFK_Exception(ss.str());
    }

    if (list.featureCount() != (list.cubeCount() * sSizes.featureCountPerCube))
        throw AFK_Exception("Insane self found");

    AFK_3DComputeUnit newUnit(
        shapeCube.location,
        shapeCube.vapourJigsawPiece,
        shapeCube.edgeJigsawPiece,
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
        shapeCube.vapourJigsawPiece,
        shapeCube.edgeJigsawPiece,
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
        if (!computer->findKernel("makeShape3DVapour", vapourKernel))
            throw AFK_Exception("Cannot find 3D vapour kernel");

    if (!edgeKernel)
        if (!computer->findKernel("makeShape3DEdge", edgeKernel))
            throw AFK_Exception("Cannot find 3D edge kernel");

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
        units.size() * sizeof(AFK_3DComputeUnit),
        &units[0], &error);
    afk_handleClError(error);

    /* TODO Remove Spam */
    for (std::vector<AFK_3DComputeUnit>::iterator cuIt = units.begin();
        cuIt != units.end(); ++cuIt)
    {
        std::cout << "Using 3D compute unit: " << *cuIt << std::endl;
    }

    /* Set up the rest of the vapour parameters */
    cl_event acquireVapourJigsawEvent = 0;
    cl_mem *vapourJigsawMem = vapourJigsaw->acquireForCl(ctxt, q, &acquireVapourJigsawEvent);

    AFK_CLCHK(clSetKernelArg(vapourKernel, 0, sizeof(cl_mem), &vapourBufs[0]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 1, sizeof(cl_mem), &vapourBufs[1]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 2, sizeof(cl_mem), &vapourBufs[2]))
    AFK_CLCHK(clSetKernelArg(vapourKernel, 3, sizeof(cl_mem), &vapourJigsawMem[0]))

    size_t vapourDim[3];
    vapourDim[0] = sSizes.vDim * unitCount;
    vapourDim[1] = vapourDim[2] = sSizes.vDim;

    /* For the edge kernel, I'm going to need the edge
     * jigsaw to have been acquired, and also the vapour
     * kernel to have completed.
     */
    cl_event vapourEvents[2];

    AFK_CLCHK(clEnqueueNDRangeKernel(q, vapourKernel, 3, NULL, &vapourDim[0], NULL,
        acquireVapourJigsawEvent ? 1 : 0,
        acquireVapourJigsawEvent ? &acquireVapourJigsawEvent : NULL,
        &vapourEvents[0]))

    if (acquireVapourJigsawEvent) AFK_CLCHK(clReleaseEvent(acquireVapourJigsawEvent))

    vapourEvents[1] = 0;
    cl_mem *edgeJigsawMem = edgeJigsaw->acquireForCl(ctxt, q, &vapourEvents[1]);

    /* Now I'm also going to need this... */
    cl_sampler vapourSampler = clCreateSampler(
        ctxt,
        CL_FALSE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_FILTER_NEAREST,
        &error);
    afk_handleClError(error);

    /* Now, I need to run the edge kernel.
     */
    AFK_CLCHK(clSetKernelArg(edgeKernel, 0, sizeof(cl_mem), &vapourJigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 1, sizeof(cl_sampler), &vapourSampler))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 2, sizeof(cl_mem), &vapourBufs[2]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 3, sizeof(cl_mem), &edgeJigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 4, sizeof(cl_mem), &edgeJigsawMem[1]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 5, sizeof(cl_mem), &edgeJigsawMem[2]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 6, sizeof(float), &sSizes.edgeThreshold))

    /* TODO: Bringing this down to global only for now and removing
     * the cross-check in an attempt to tackle the
     * repeated freezing.
     */
    size_t edgeGlobalDim[3];
    edgeGlobalDim[0] = 6 * unitCount;
    edgeGlobalDim[1] = edgeGlobalDim[2] = sSizes.eDim;

#if 0
    size_t edgeLocalDim[3];
    edgeLocalDim[0] = 6; /* one for each of the 6 face orientations */
    edgeLocalDim[1] = edgeLocalDim[2] = sSizes.eDim;
#endif

    cl_event edgeEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, edgeKernel, 3, 0, &edgeGlobalDim[0], /* &edgeLocalDim[0] */ NULL,
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

    f.clear();
    c.clear();
    units.clear();
}

