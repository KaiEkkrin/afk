/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "landscape_compute_queue.hpp"


/* AFK_TerrainComputeUnit implementation */

AFK_TerrainComputeUnit::AFK_TerrainComputeUnit(
    unsigned int _tileOffset,
    unsigned int _tileCount,
    unsigned int _piece):
        tileOffset(_tileOffset),
        tileCount(_tileCount),
        piece(_piece)
{
}


/* AFK_TerrainComputeQueue implementation */

AFK_TerrainComputeQueue::AFK_TerrainComputeQueue(const AFK_LandscapeSizes& lSizes):
    AFK_TerrainList(lSizes)
{
}

void AFK_TerrainComputeQueue::extend(const AFK_TerrainList& list, unsigned int piece)
{
    boost::unique_lock<boost::mutex> lock(mut);

    AFK_TerrainComputeUnit newUnit(
        t.size(),
        list.t.size(),
        piece);
    AFK_TerrainList::extend(list.f, list.t);
    units.push_back(newUnit);
}

unsigned int AFK_TerrainComputeQueue::getUnitCount(void) const
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.size();
}

AFK_TerrainComputeUnit AFK_TerrainComputeQueue::getUnit(unsigned int unitIndex) const
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units[unitIndex];
}

void AFK_TerrainComputeQueue::copyToClBuffers(cl_context ctxt, cl_mem *mem)
{
    boost::unique_lock<boost::mutex> lock(mut);

    cl_int error;

    mem[0] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
        f.size() * sizeof(AFK_TerrainFeature),
        &f[0], &error);
    afk_handleClError(error);

    mem[1] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
        t.size() * sizeof(AFK_TerrainTile),
        &t[0], &error);
    afk_handleClError(error);

    mem[2] = clCreateBuffer(
        ctxt, CL_MEM_READ_ONLY | CL_MEM_USE_HOST_PTR,
        units.size() * sizeof(AFK_TerrainComputeUnit),
        &units[0], &error);
    afk_handleClError(error);
}

void AFK_LandscapeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    f.clear();
    t.clear();
    units.clear();
}


/* AFK_TerrainComputeFair implementation */


AFK_TerrainComputeFair::AFK_TerrainComputeFair():
    updateInc(0), drawInc(1)
{ 
}

boost::shared_ptr<AFK_TerrainComputeQueue> AFK_TerrainComputeFair::getUpdateQueue(
    const AFK_JigsawPiece& jigsawPiece,
    const AFK_LandscapeSizes& lSizes)
{
    boost::unique_lock<boost::mutex> lock(mut);

    unsigned int qIndex = jigsawPiece.puzzle * 2 + updateInc;
    while (queues.size() <= qIndex)
    {
        boost::shared_ptr<AFK_TerrainComputeQueue> newQ(
            new AFK_TerrainComputeQueue(lSizes));
        queues.push_back(newQ);
    }

    return queues[qIndex];
}

void AFK_TerrainComputeFair::getDrawQueues(std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> >& drawQueues)
{
    boost::unique_lock<boost::mutex> lock(mut);

    for (unsigned int dI = drawInc; dI < queues.size(); dI += 2)
        drawQueues.push_back(queues[dI]);
}

void AFK_TerrainComputeFair::flipQueues(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    updateInc = (updateInc == 0 ? 1 : 0);
    drawInc = (drawInc == 0 ? 1 : 0);

    for (unsigned int uI = updateInc; uI < queues.size(); uI += 2)
        queues[uI]->clear();
}

