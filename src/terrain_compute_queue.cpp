/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <sstream>

#include "computer.hpp"
#include "exception.hpp"
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

AFK_TerrainComputeUnit AFK_TerrainComputeQueue::extend(const AFK_TerrainList& list, const Vec2<int>& piece, const AFK_LandscapeSizes& lSizes)
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

int AFK_TerrainComputeQueue::getUnitCount(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.size();
}

AFK_TerrainComputeUnit AFK_TerrainComputeQueue::getUnit(int unitIndex)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units[unitIndex];
}

bool AFK_TerrainComputeQueue::empty(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    return units.empty();
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

void AFK_TerrainComputeQueue::clear(void)
{
    boost::unique_lock<boost::mutex> lock(mut);

    f.clear();
    t.clear();
    units.clear();
}

