/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <algorithm>

#include "3d_edge_compute_queue.hpp"
#include "exception.hpp"


/* AFK_3DEdgeComputeUnit implementation */

AFK_3DEdgeComputeUnit::AFK_3DEdgeComputeUnit(
    const Vec4<float>& _location,
    const AFK_JigsawPiece *_vapourJigsawPieces,
    const AFK_JigsawPiece& _edgeJigsawPiece):
        location(_location)
{
    for (int i = 0; i < AFK_3DECU_VPCOUNT; ++i)
    {
	    vapourPiece[i] = afk_vec4<int>(
		    _vapourJigsawPieces[i].u,
    		_vapourJigsawPieces[i].v,
    		_vapourJigsawPieces[i].w,
    		_vapourJigsawPieces[i].puzzle);
    }

	edgePiece = afk_vec2<int>(
		_edgeJigsawPiece.u,
		_edgeJigsawPiece.v);
}

std::ostream& operator<<(std::ostream& os, const AFK_3DEdgeComputeUnit& unit)
{
    os << "(SCU: ";
    os << "location=" << std::dec << unit.location;
    os << ", vapourPieces=(";
    for (int i = 0; i < AFK_3DECU_VPCOUNT; ++i)
    {
        if (i > 0) os << ", ";
        os << unit.vapourPiece[i];
    }
    os << "), edgePiece=" << unit.edgePiece;
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
    const AFK_JigsawPiece *vapourJigsawPieces,
    const AFK_JigsawPiece& edgeJigsawPiece)
{
    boost::unique_lock<boost::mutex> lock(mut);

    AFK_3DEdgeComputeUnit newUnit(
        location,
        vapourJigsawPieces,
        edgeJigsawPiece);
    units.push_back(newUnit);
    return newUnit;
}

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
    int jpI, jpCount;
    jpCount = vapourJigsaws->getPuzzleCount();
    if (jpCount > 4) throw AFK_Exception("Too many vapour jigsaws");
    for (jpI = 0; jpI < jpCount; ++jpI)
        vapourJigsawsMem[jpI] = vapourJigsaws->getPuzzle(jpI)->acquireForCl(ctxt, q, preEdgeWaitList);
    for (; jpI < 4; ++jpI)
        vapourJigsawsMem[jpI] = NULL;

    cl_mem *edgeJigsawMem = edgeJigsaw->acquireForCl(ctxt, q, preEdgeWaitList);

    /* Now, I need to run the edge kernel.
     * Note that vapour jigsaws that don't exist yet won't ever
     * be referred to in the pieces list so it doesn't matter
     * what I set as the argument, but I must set something
     * valid, because OpenCL doesn't support NULL image
     * arguments: I'll use the first jigsaw, which will always
     * exist.
     */
    for (jpI = 0; jpI < 4; ++jpI)
        if (vapourJigsawsMem[jpI])
            AFK_CLCHK(clSetKernelArg(edgeKernel, jpI, sizeof(cl_mem), vapourJigsawsMem[jpI]))
        else
            AFK_CLCHK(clSetKernelArg(edgeKernel, jpI, sizeof(cl_mem), vapourJigsawsMem[0]))

    AFK_CLCHK(clSetKernelArg(edgeKernel, 4, sizeof(cl_mem), &unitsBuf))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 5, sizeof(cl_mem), &edgeJigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 6, sizeof(cl_mem), &edgeJigsawMem[1]))
    AFK_CLCHK(clSetKernelArg(edgeKernel, 7, sizeof(cl_mem), &edgeJigsawMem[2]))

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
    for (jpI = 0; jpI < jpCount; ++jpI)
        vapourJigsaws->getPuzzle(jpI)->releaseFromCl(q, postEdgeWaitList);
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

