/* AFK
 * Copyright (C) 2013, Alex Holloway.
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

#include <cassert>

#include "core.hpp"
#include "debug.hpp"
#include "rng/boost_taus88.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"


AFK_KeyedCell afk_shapeToVapourCell(const AFK_KeyedCell& cell, const AFK_ShapeSizes& sSizes)
{
    return cell.parent(sSizes.skeletonFlagGridDim);
}


/* AFK_VapourCell implementation. */

AFK_VapourCell::AFK_VapourCell():
    skeleton(nullptr),
    features(nullptr),
    cubes(nullptr)
{
}

AFK_VapourCell::~AFK_VapourCell()
{
    evict();
}

bool AFK_VapourCell::hasDescriptor(void) const
{
    return haveDescriptor;
}

void AFK_VapourCell::makeDescriptor(
    const AFK_ShapeSizes& sSizes)
{
    if (!haveDescriptor)
    {
        AFK_KeyedCell cell = getCell();

        assert(!skeleton);
        skeleton = new AFK_Skeleton();

        assert(!features);
        features = new std::vector<AFK_3DVapourFeature>();

        assert(!cubes);
        cubes = new std::vector<AFK_3DVapourCube>();

        AFK_Boost_Taus88_RNG rng;

        rng.seed(cell.rngSeed());
        skeleton->make(rng, sSizes);

        AFK_3DVapourCube cube;
        cube.make(
            *features,
            cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
            *skeleton,
            sSizes,
            rng);
        cubes->push_back(cube);

        haveDescriptor = true;
    }
}

void AFK_VapourCell::makeDescriptor(
    const AFK_VapourCell& upperVapourCell,
    const AFK_ShapeSizes& sSizes)
{
    /* Sanity check. */
    if (!upperVapourCell.hasDescriptor()) throw AFK_Exception("Vapour cell descriptors built in wrong order");

    if (!haveDescriptor)
    {
        AFK_KeyedCell cell = getCell();
        AFK_KeyedCell upperCell = upperVapourCell.getCell();

        assert(!skeleton);
        skeleton = new AFK_Skeleton();

        assert(!features);
        features = new std::vector<AFK_3DVapourFeature>();

        assert(!cubes);
        cubes = new std::vector<AFK_3DVapourCube>();

        AFK_Boost_Taus88_RNG rng;

        rng.seed(cell.rngSeed());

        Vec3<int64_t> thisCellShapeSpace = afk_vec3<int64_t>(
            cell.c.coord.v[0], cell.c.coord.v[1], cell.c.coord.v[2]);
        Vec3<int64_t> upperCellShapeSpace = afk_vec3<int64_t>(
            upperCell.c.coord.v[0], upperCell.c.coord.v[1], upperCell.c.coord.v[2]);
        Vec3<int64_t> upperOffset = (thisCellShapeSpace - upperCellShapeSpace) * (sSizes.skeletonFlagGridDim/2) / cell.c.coord.v[3];

        if (skeleton->make(
            *(upperVapourCell.skeleton),
            upperOffset,
            rng,
            sSizes) > 0)
        {
            AFK_3DVapourCube cube;
            cube.make(
                *features,
                cell.toWorldSpace(SHAPE_CELL_WORLD_SCALE),
                *skeleton,
                sSizes,
                rng);
            cubes->push_back(cube);
        }

        haveDescriptor = true;
    }
}

bool AFK_VapourCell::withinSkeleton(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const
{
    assert(skeleton);
    assert(shapeCell.c.coord.v[3] == getCell().c.coord.v[3] / sSizes.skeletonFlagGridDim);

    AFK_SkeletonCube cube(key, shapeCell, sSizes);
    return skeleton->within(cube);
}

int AFK_VapourCell::skeletonAdjacency(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const
{
    assert(skeleton);
    assert(shapeCell.c.coord.v[3] == getCell().c.coord.v[3] / sSizes.skeletonFlagGridDim);

    AFK_SkeletonCube cube(key, shapeCell, sSizes);
    return skeleton->getAdjacency(cube);
}

int AFK_VapourCell::skeletonFullAdjacency(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const
{
    assert(skeleton);
    assert(shapeCell.c.coord.v[3] == getCell().c.coord.v[3] / sSizes.skeletonFlagGridDim);

    AFK_SkeletonCube cube(key, shapeCell, sSizes);
    return skeleton->getFullAdjacency(cube);
}

AFK_VapourCell::ShapeCells::ShapeCells(const AFK_VapourCell& _vapourCell, const AFK_ShapeSizes& _sSizes):
    bones(AFK_Skeleton::Bones(*(_vapourCell.skeleton))),
    vapourCell(_vapourCell),
    sSizes(_sSizes)
{
}

bool AFK_VapourCell::ShapeCells::hasNext(void)
{
    return bones.hasNext();
}

AFK_KeyedCell AFK_VapourCell::ShapeCells::next(void)
{
    AFK_SkeletonCube nextSkeletonCube = bones.next();
    AFK_KeyedCell nextCell = nextSkeletonCube.toShapeCell(vapourCell.getCell(), sSizes);

    return nextCell;
}

void AFK_VapourCell::build3DList(
    unsigned int threadId,
    AFK_3DList& list,
    const AFK_ShapeSizes& sSizes,
    const AFK_VAPOUR_CELL_CACHE *cache) const
{
    /* Add the local vapour to the list. */
    list.extend(*features, *cubes);

    AFK_KeyedCell currentCell = getCell();

    /* If this isn't the top level cell... */
    if (currentCell.c.coord.v[3] < (sSizes.skeletonFlagGridDim * SHAPE_CELL_MAX_DISTANCE))
    {
        /* Pull the parent cell from the cache, and
         * include its list too
         */
        AFK_KeyedCell parentCell = currentCell.parent(sSizes.subdivisionFactor);
        AFK_VapourCell& parentVapourCell = cache->at(parentCell);
        enum AFK_ClaimStatus claimStatus = parentVapourCell.claimable.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);
        if (claimStatus != AFK_CL_CLAIMED_SHARED && claimStatus != AFK_CL_CLAIMED_UPGRADABLE)
        {
            std::ostringstream ss;
            ss << "Unable to claim VapourCell at " << parentCell << ": got status " << claimStatus;
            throw AFK_Exception(ss.str());
        }
        parentVapourCell.build3DList(threadId, list, sSizes, cache);
        parentVapourCell.claimable.release(threadId, claimStatus);
    }
}

bool AFK_VapourCell::alreadyEnqueued(
    unsigned int& o_cubeOffset,
    unsigned int& o_cubeCount) const
{
    if (computeCubeFrame == afk_core.computingFrame)
    {
        o_cubeOffset = computeCubeOffset;
        o_cubeCount = computeCubeCount;
        return true;
    }
    else return false;
}

void AFK_VapourCell::enqueued(
    unsigned int cubeOffset,
    unsigned int cubeCount)
{
    /* Sanity check */
    if (computeCubeFrame == afk_core.computingFrame)
        throw AFK_Exception("Vapour cell enqueue conflict");

    computeCubeOffset = cubeOffset;
    computeCubeCount = cubeCount;
    computeCubeFrame = afk_core.computingFrame;
}

void AFK_VapourCell::evict(void)
{
    if (cubes)
    {
        delete cubes;
        cubes = nullptr;
    }

    if (features)
    {
        delete features;
        features = nullptr;
    }

    if (skeleton)
    {
        delete skeleton;
        skeleton = nullptr;
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell)
{
    os << "Vapour cell at " << vapourCell.key.load() << " with " <<
        (vapourCell.features ? vapourCell.features->size() : 0) << " features and " <<
        (vapourCell.cubes ? vapourCell.cubes->size() : 0) << " cubes";
    return os;
}

