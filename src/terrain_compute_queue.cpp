/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <sstream>

#include "computer.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "terrain_compute_queue.hpp"


/* AFK_TerrainComputeUnit implementation */

AFK_TerrainComputeUnit::AFK_TerrainComputeUnit(
    int _tileOffset,
    int _tileCount,
    const Vec2<int>& _piece):
        tileOffset(_tileOffset),
        tileCount(_tileCount),
        piece(_piece)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_TerrainComputeUnit& unit)
{
    os << "(TCU: ";
    os << "tileOffset=" << std::dec << unit.tileOffset;
    os << ", tileCount=" << std::dec << unit.tileCount;
    os << ", piece=" << std::dec << unit.piece;
    os << ")";
    return os;
}


/* AFK_TerrainComputeQueue implementation */

AFK_TerrainComputeQueue::AFK_TerrainComputeQueue():
    terrainKernel(0), surfaceKernel(0), yReduce(NULL)
{
}

AFK_TerrainComputeQueue::~AFK_TerrainComputeQueue()
{
    if (yReduce) delete yReduce;
}

AFK_TerrainComputeUnit AFK_TerrainComputeQueue::extend(const AFK_TerrainList& list, const Vec2<int>& piece, AFK_LandscapeTile *landscapeTile, const AFK_LandscapeSizes& lSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);

    /* Make sure we're not pushing empties, that's a bug. */
    if (list.tileCount() == 0)
    {
        std::ostringstream ss;
        ss << "Pushed empty list to terrain compute queue for piece " << std::dec << piece;
        throw AFK_Exception(ss.str());
    }

    /* Make sure the list is sensible */
    if (list.featureCount() != (list.tileCount() * lSizes.featureCountPerTile)) throw AFK_Exception("Insane terrain list found");

    /* Make sure *WE* are sensible */
    if (AFK_TerrainList::featureCount() != (AFK_TerrainList::tileCount() * lSizes.featureCountPerTile))
        throw AFK_Exception("Insane self found");

    AFK_TerrainComputeUnit newUnit(
        AFK_TerrainList::tileCount(),
        list.tileCount(),
        piece);
    AFK_TerrainList::extend(list);
    units.push_back(newUnit);
	landscapeTiles.push_back(landscapeTile);
    return newUnit;
}

std::string AFK_TerrainComputeQueue::debugTerrain(const AFK_TerrainComputeUnit& unit, const AFK_LandscapeSizes& lSizes) const
{
    std::ostringstream ss;
    ss << std::endl;
    for (int i = unit.tileOffset; i < (unit.tileOffset + unit.tileCount); ++i)
    {
        ss << "  " << t[i] << ":" << std::endl;
        for (unsigned int j = i * lSizes.featureCountPerTile;
            j < ((i + 1) * lSizes.featureCountPerTile);
            ++j)
        {
            ss << "    " << f[j] << std::endl;
        }
    }

    return ss.str();
}

void AFK_TerrainComputeQueue::computeStart(
    AFK_Computer *computer,
    AFK_Jigsaw *jigsaw,
    const AFK_LandscapeSizes& lSizes,
    const Vec3<float>& baseColour)
{
    boost::unique_lock<boost::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Make sure the compute stuff is initialised... */
    if (!terrainKernel)
        if (!computer->findKernel("makeLandscapeTerrain", terrainKernel))
            throw AFK_Exception("Cannot find terrain kernel");

    if (!surfaceKernel)
        if (!computer->findKernel("makeLandscapeSurface", surfaceKernel))
            throw AFK_Exception("Cannot find surface kernel");

    if (!yReduce)
        yReduce = new AFK_YReduce(computer);

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    /* Copy the terrain inputs to CL buffers. */
    cl_mem terrainBufs[3];

    terrainBufs[0] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        f.size() * sizeof(AFK_TerrainFeature),
        &f[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    terrainBufs[1] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        t.size() * sizeof(AFK_TerrainTile),
        &t[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    terrainBufs[2] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
        units.size() * sizeof(AFK_TerrainComputeUnit),
        &units[0], &error);
    AFK_HANDLE_CL_ERROR(error);

    /* Set up the rest of the terrain parameters */
    Vec4<float> baseColour4 = afk_vec4<float>(baseColour, 0.0f);

    preTerrainWaitList.clear();
    cl_mem *jigsawMem = jigsaw->acquireForCl(ctxt, q, preTerrainWaitList);

    AFK_CLCHK(clSetKernelArg(terrainKernel, 0, sizeof(cl_mem), &terrainBufs[0]))
    AFK_CLCHK(clSetKernelArg(terrainKernel, 1, sizeof(cl_mem), &terrainBufs[1]))
    AFK_CLCHK(clSetKernelArg(terrainKernel, 2, sizeof(cl_mem), &terrainBufs[2]))
    AFK_CLCHK(clSetKernelArg(terrainKernel, 3, sizeof(cl_float4), &baseColour4.v[0]))
    AFK_CLCHK(clSetKernelArg(terrainKernel, 4, sizeof(cl_mem), &jigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(terrainKernel, 5, sizeof(cl_mem), &jigsawMem[1]))

    size_t terrainDim[3];
    terrainDim[0] = terrainDim[1] = lSizes.tDim;
    terrainDim[2] = unitCount;

    cl_event terrainEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, terrainKernel, 3, NULL, &terrainDim[0], NULL,
        preTerrainWaitList.size(),
        &preTerrainWaitList[0],
        &terrainEvent))

    for (std::vector<cl_event>::iterator evIt = preTerrainWaitList.begin();
        evIt != preTerrainWaitList.end(); ++evIt)
    {
        AFK_CLCHK(clReleaseEvent(*evIt))
    }

    /* For the next two I'm going to need this ...
     */
    cl_sampler jigsawYDispSampler = clCreateSampler(
        ctxt,
        CL_FALSE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_FILTER_NEAREST,
        &error);
    AFK_HANDLE_CL_ERROR(error);

    /* Now, I need to run the kernel to bake the surface normals.
     */
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 0, sizeof(cl_mem), &terrainBufs[2]))
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 1, sizeof(cl_mem), &jigsawMem[0]))
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 2, sizeof(cl_sampler), &jigsawYDispSampler))
    AFK_CLCHK(clSetKernelArg(surfaceKernel, 3, sizeof(cl_mem), &jigsawMem[2]))

    size_t surfaceGlobalDim[3];
    surfaceGlobalDim[0] = surfaceGlobalDim[1] = lSizes.tDim - 1;
    surfaceGlobalDim[2] = unitCount;

    size_t surfaceLocalDim[3];
    surfaceLocalDim[0] = surfaceLocalDim[1] = lSizes.tDim - 1;
    surfaceLocalDim[2] = 1;

    cl_event surfaceEvent, yReduceEvent;

    AFK_CLCHK(clEnqueueNDRangeKernel(q, surfaceKernel, 3, 0, &surfaceGlobalDim[0], &surfaceLocalDim[0],
        1, &terrainEvent, &surfaceEvent))

    /* Finally, do the y reduce. */
    yReduce->compute(
        ctxt,
        q,
        unitCount,
        &terrainBufs[2],
        &jigsawMem[0],
        &jigsawYDispSampler,
        lSizes,
        1,
        &terrainEvent,
        &yReduceEvent);

    postTerrainWaitList.clear();
    postTerrainWaitList.push_back(surfaceEvent);
    postTerrainWaitList.push_back(yReduceEvent);

    /* Release the things */
    AFK_CLCHK(clReleaseSampler(jigsawYDispSampler))
    AFK_CLCHK(clReleaseEvent(terrainEvent))

    for (unsigned int i = 0; i < 3; ++i)
    {
        AFK_CLCHK(clReleaseMemObject(terrainBufs[i]))
    }

    jigsaw->releaseFromCl(q, postTerrainWaitList);
    AFK_CLCHK(clReleaseEvent(surfaceEvent))
    AFK_CLCHK(clReleaseEvent(yReduceEvent))

    computer->unlock();
}

void AFK_TerrainComputeQueue::computeFinish(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    unsigned int unitCount = units.size();
    if (unitCount == 0) return;

    /* Read back the Y reduce. */
    yReduce->readBack(unitCount, landscapeTiles);
}

bool AFK_TerrainComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
}

void AFK_TerrainComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    f.clear();
    t.clear();
    units.clear();
	landscapeTiles.clear();
}

