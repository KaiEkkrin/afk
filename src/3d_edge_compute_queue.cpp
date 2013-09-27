/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <algorithm>

#include "3d_edge_compute_queue.hpp"
#include "exception.hpp"


/* AFK_3DEdgeComputeUnit implementation */

AFK_3DEdgeComputeUnit::AFK_3DEdgeComputeUnit(
    const Vec4<float>& _location,
    const AFK_JigsawPiece& _vapourJigsawPiece,
    const AFK_JigsawPiece& _edgeJigsawPiece):
        location(_location)
{
    vapourPiece = afk_vec4<int>(
        _vapourJigsawPiece.u,
        _vapourJigsawPiece.v,
        _vapourJigsawPiece.w,
        _vapourJigsawPiece.puzzle);

	edgePiece = afk_vec2<int>(
		_edgeJigsawPiece.u,
		_edgeJigsawPiece.v);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << unit.location;
    os << ", vapourPiece=" << unit.vapourPiece;
    os << ", edgePiece=" << unit.edgePiece;
    os << ")";
    return os;
}


/* AFK_3DEdgeComputeQueue implementation */

AFK_3DEdgeComputeQueue::AFK_3DEdgeComputeQueue():
    edgeKernel(0)
{
}

AFK_3DEdgeComputeQueue::~AFK_3DEdgeComputeQueue()
{
}

AFK_3DEdgeComputeUnit AFK_3DEdgeComputeQueue::append(
    const Vec4<float>& location,
    const AFK_JigsawPiece& vapourJigsawPiece,
    const AFK_JigsawPiece& edgeJigsawPiece)
{
    boost::unique_lock<boost::mutex> lock(mut);

    AFK_3DEdgeComputeUnit newUnit(
        location,
        vapourJigsawPiece,
        edgeJigsawPiece);
    units.push_back(newUnit);
    return newUnit;
}

#define CULL_COMMON_POINTS 1

void AFK_3DEdgeComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_Jigsaw *edgeJigsaw,
    const AFK_ShapeSizes& sSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!edgeKernel)
        if (!computer->findKernel("makeShape3DEdge", edgeKernel))
            throw AFK_Exception("Cannot find 3D edge kernel");

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the unit list to a CL buffer. */
    cl_mem unitsBuf = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_3DEdgeComputeUnit),
        &units[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    /* Set up the rest of the parameters */
    preEdgeWaitList.clear();
    cl_mem *vapourJigsawsMem[4];
    int jpCount = vapourJigsaws->acquireAllForCl(ctxt, q, vapourJigsawsMem, 4, preEdgeWaitList);
    cl_mem *edgeJigsawMem = edgeJigsaw->acquireForCl(ctxt, q, preEdgeWaitList);

    for (int jpI = 0; jpI < 4; ++jpI)
        AFK_CLCHK(clSetKernelArg(edgeKernel, jpI, sizeof(cl_mem), vapourJigsawsMem[jpI]))

    AFK_CLCHK(clSetKernelArg(edgeKernel, 4, sizeof(cl_mem), &unitsBuf))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 5, sizeof(cl_mem), &edgeJigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 6, sizeof(cl_mem), &edgeJigsawMem[1]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 7, sizeof(cl_mem), &edgeJigsawMem[2]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 8, sizeof(cl_mem), &edgeJigsawMem[3]))

    size_t edgeGlobalDim[3];
    edgeGlobalDim[0] = unitCount;
    edgeGlobalDim[1] = edgeGlobalDim[2] = sSizes.eDim;

#if CULL_COMMON_POINTS
    size_t edgeLocalDim[3];
    edgeLocalDim[0] = 1;
    edgeLocalDim[1] = edgeLocalDim[2] = sSizes.eDim;
#endif

    cl_event edgeEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, edgeKernel, 3, 0, &edgeGlobalDim[0],
#if CULL_COMMON_POINTS
        &edgeLocalDim[0],
#else
        NULL,
#endif
        preEdgeWaitList.size(),
        &preEdgeWaitList[0],
        &edgeEvent))

    for (std::vector<cl_event>::iterator evIt = preEdgeWaitList.begin();
        evIt != preEdgeWaitList.end(); ++evIt)
    {
        AFK_CLCHK(clReleaseEvent(*evIt))
    }

    AFK_CLCHK(clReleaseMemObject(unitsBuf))

    postEdgeWaitList.clear();
    postEdgeWaitList.push_back(edgeEvent);
    vapourJigsaws->releaseAllFromCl(q, vapourJigsawsMem, jpCount, postEdgeWaitList);
    edgeJigsaw->releaseFromCl(q, postEdgeWaitList);
    AFK_CLCHK(clReleaseEvent(edgeEvent))

    computer->unlock();
}

void AFK_3DEdgeComputeQueue::computeFinish(void)
{
    /* This method is included for symmetry with TerrainComputeQueue.
     * Right now it does nothing.
     */
}

bool AFK_3DEdgeComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
}

void AFK_3DEdgeComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    units.clear();
}

