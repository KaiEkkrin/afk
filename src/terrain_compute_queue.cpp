/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#include "afk.hpp"

#include <sstream>

#include "compute_dependency.hpp"
#include "compute_queue.hpp"
#include "computer.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "terrain_compute_queue.hpp"
#include "world.hpp"


/* AFK_TerrainComputeUnit implementation */

AFK_TerrainComputeUnit::AFK_TerrainComputeUnit(
    size_t _tileOffset,
    size_t _tileCount,
    const Vec2<int>& _piece):
        piece(_piece)
{
    /* I don't want to use 64 bits for tile offset and count, would take up
     * extra space for no good reason
     */
    assert(_tileOffset <= INT32_MAX);
    assert(_tileCount <= INT32_MAX);

    tileOffset = static_cast<int>(_tileOffset);
    tileCount = static_cast<int>(_tileCount);
}

std::ostream& operator<<(std::ostream& os, const AFK_TerrainComputeUnit& unit)
{
    os << "(TCU: ";
    os << "tileOffset=" << std::dec << unit.tileOffset;
    os << ", tileCount=" << std::dec << unit.tileCount;
    os << ", piece=" << std::dec << afk_vec2<int, cl_int2>(unit.piece);
    os << ")";
    return os;
}


/* AFK_TerrainComputeQueue implementation */

AFK_TerrainComputeQueue::AFK_TerrainComputeQueue():
    terrainKernel(0), surfaceKernel(0), yReduce(nullptr), postTerrainDep(nullptr)
{
}

AFK_TerrainComputeQueue::~AFK_TerrainComputeQueue()
{
    if (yReduce) delete yReduce;
    if (postTerrainDep) delete postTerrainDep;
}

AFK_TerrainComputeUnit AFK_TerrainComputeQueue::extend(const AFK_TerrainList& list, const Vec2<int>& piece, const AFK_Tile& tile, const AFK_LandscapeSizes& lSizes)
{
    std::unique_lock<std::mutex> lock(mut);

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
	landscapeTiles.push_back(tile);
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
    std::unique_lock<std::mutex> lock(mut);
    cl_int error;

    /* Check there's something to do */
    size_t unitCount = units.size();
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

    cl_context ctxt = computer->getContext();
    auto kernelQueue = computer->getKernelQueue();
    auto writeQueue = computer->getWriteQueue();

    /* Copy the terrain inputs to CL buffers. */
    AFK_ComputeDependency noDep(computer);
    AFK_ComputeDependency preTerrainDep(computer);

    cl_mem terrainBufs[3] = {
        featureInput.bufferData(f.data(), f.size() * sizeof(AFK_TerrainFeature), computer, noDep, preTerrainDep),
        tileInput.bufferData(t.data(), t.size() * sizeof(AFK_TerrainTile), computer, noDep, preTerrainDep),
        unitInput.bufferData(units.data(), units.size() * sizeof(AFK_TerrainComputeUnit), computer, noDep, preTerrainDep)
    };

    /* Set up the rest of the terrain parameters */
    Vec4<float> baseColour4 = afk_vec4<float>(baseColour, 0.0f);

    jigsaw->setupImages(computer);
    cl_mem jigsawYDispMem = jigsaw->acquireForCl(0, preTerrainDep);
    cl_mem jigsawColourMem = jigsaw->acquireForCl(1, preTerrainDep);

    kernelQueue->kernel(terrainKernel);
    
    for (int tbI = 0; tbI < 3; ++tbI)
        kernelQueue->kernelArg(sizeof(cl_mem), &terrainBufs[tbI]);

    kernelQueue->kernelArg(sizeof(cl_float4), &baseColour4.v[0]);
    kernelQueue->kernelArg(sizeof(cl_mem), &jigsawYDispMem);
    kernelQueue->kernelArg(sizeof(cl_mem), &jigsawColourMem);

    size_t terrainDim[3];
    terrainDim[0] = terrainDim[1] = lSizes.tDim;
    terrainDim[2] = unitCount;

    AFK_ComputeDependency preSurfaceDep(computer);
    kernelQueue->kernel3D(terrainDim, nullptr, preTerrainDep, preSurfaceDep);

    /* For the next two I'm going to need this ...
     */
    cl_sampler jigsawYDispSampler = computer->oclShim.CreateSampler()(
        ctxt,
        CL_FALSE,
        CL_ADDRESS_CLAMP_TO_EDGE,
        CL_FILTER_NEAREST,
        &error);
    AFK_HANDLE_CL_ERROR(error);

    cl_mem jigsawNormalMem = jigsaw->acquireForCl(2, preSurfaceDep);

    /* Now, I need to run the kernel to bake the surface normals.
     */
    kernelQueue->kernel(surfaceKernel);
    kernelQueue->kernelArg(sizeof(cl_mem), &terrainBufs[2]);
    kernelQueue->kernelArg(sizeof(cl_mem), &jigsawYDispMem);
    kernelQueue->kernelArg(sizeof(cl_sampler), &jigsawYDispSampler);
    kernelQueue->kernelArg(sizeof(cl_mem), &jigsawNormalMem);

    size_t surfaceGlobalDim[3];
    surfaceGlobalDim[0] = surfaceGlobalDim[1] = lSizes.tDim - 1;
    surfaceGlobalDim[2] = unitCount;

    size_t surfaceLocalDim[3];
    surfaceLocalDim[0] = surfaceLocalDim[1] = lSizes.tDim - 1;
    surfaceLocalDim[2] = 1;

    if (!postTerrainDep) postTerrainDep = new AFK_ComputeDependency(computer);
    assert(postTerrainDep->getEventCount() == 0);
    kernelQueue->kernel3D(surfaceGlobalDim, surfaceLocalDim, preSurfaceDep, *postTerrainDep);

    /* Finally, do the y reduce. */
    yReduce->compute(
        unitCount,
        &terrainBufs[2],
        &jigsawYDispMem,
        &jigsawYDispSampler,
        lSizes,
        preSurfaceDep,
        *postTerrainDep);

    /* Release the things */
    AFK_CLCHK(computer->oclShim.ReleaseSampler()(jigsawYDispSampler));
}

void AFK_TerrainComputeQueue::computeFinish(unsigned int threadId, AFK_Jigsaw *jigsaw, AFK_LANDSCAPE_CACHE *cache)
{
    std::unique_lock<std::mutex> lock(mut);

    size_t unitCount = units.size();
    if (unitCount == 0) return;

    /* Release the images. */
    assert(postTerrainDep && yReduce);
    jigsaw->releaseFromCl(0, *postTerrainDep);
    jigsaw->releaseFromCl(1, *postTerrainDep);
    jigsaw->releaseFromCl(2, *postTerrainDep);

    /* Read back the Y reduce. */
    yReduce->readBack(threadId, landscapeTiles, cache);
}

bool AFK_TerrainComputeQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return units.empty();
}

void AFK_TerrainComputeQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    if (postTerrainDep) postTerrainDep->waitFor();

    f.clear();
    t.clear();
    units.clear();
	landscapeTiles.clear();
}

