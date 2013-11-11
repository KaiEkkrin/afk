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
#include <cstring>

#include "skeleton.hpp"



/* AFK_SkeletonCube implementation */

AFK_SkeletonCube::AFK_SkeletonCube()
{
    coord = afk_vec3<int64_t>(0LL, 0LL, 0LL);
}

AFK_SkeletonCube::AFK_SkeletonCube(const Vec3<int64_t>& _coord):
    coord(_coord)
{
}

AFK_SkeletonCube::AFK_SkeletonCube(const AFK_KeyedCell& vapourCell, const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes)
{
    int64_t shapeCellScale = vapourCell.c.coord.v[3] / sSizes.skeletonFlagGridDim;
    coord = afk_vec3<int64_t>(
        (shapeCell.c.coord.v[0] - vapourCell.c.coord.v[0]) / shapeCellScale,
        (shapeCell.c.coord.v[1] - vapourCell.c.coord.v[1]) / shapeCellScale,
        (shapeCell.c.coord.v[2] - vapourCell.c.coord.v[2]) / shapeCellScale);
}

AFK_SkeletonCube AFK_SkeletonCube::adjacentCube(int face) const
{
    AFK_SkeletonCube adj;

    switch (face)
    {
    case 0: /* bottom */
        adj.coord = afk_vec3<int64_t>(coord.v[0], coord.v[1] - 1, coord.v[2]);
        break;

    case 1: /* left */
        adj.coord = afk_vec3<int64_t>(coord.v[0] - 1, coord.v[1], coord.v[2]);
        break;

    case 2: /* front */
        adj.coord = afk_vec3<int64_t>(coord.v[0], coord.v[1], coord.v[2] - 1);
        break;

    case 3: /* back */
        adj.coord = afk_vec3<int64_t>(coord.v[0], coord.v[1], coord.v[2] + 1);
        break;

    case 4: /* right */
        adj.coord = afk_vec3<int64_t>(coord.v[0] + 1, coord.v[1], coord.v[2]);
        break;

    case 5: /* top */
        adj.coord = afk_vec3<int64_t>(coord.v[0], coord.v[1] + 1, coord.v[2]);
        break;

    default:
        throw AFK_Exception("Invalid face");
    }

    return adj;
}

AFK_SkeletonCube AFK_SkeletonCube::upperCube(const Vec3<int64_t>& upperOffset, unsigned int subdivisionFactor) const
{
    return AFK_SkeletonCube(afk_vec3<int64_t>(
        coord.v[0] / subdivisionFactor,
        coord.v[1] / subdivisionFactor,
        coord.v[2] / subdivisionFactor) + upperOffset);
}

AFK_KeyedCell AFK_SkeletonCube::toShapeCell(const AFK_KeyedCell& vapourCell, const AFK_ShapeSizes& sSizes) const
{
    int64_t shapeCellScale = vapourCell.c.coord.v[3] / sSizes.skeletonFlagGridDim;
    return afk_keyedCell(afk_vec4<int64_t>(
        coord.v[0] * shapeCellScale + vapourCell.c.coord.v[0],
        coord.v[1] * shapeCellScale + vapourCell.c.coord.v[1],
        coord.v[2] * shapeCellScale + vapourCell.c.coord.v[2],
        shapeCellScale), vapourCell.key);
}

void AFK_SkeletonCube::advance(int gridDim)
{
    ++(coord.v[0]);
    if (coord.v[0] == gridDim)
    {
        coord.v[0] = 0;
        ++(coord.v[1]);

        if (coord.v[1] == gridDim)
        {
            coord.v[1] = 0;
            ++(coord.v[2]);
        }
    }
}

bool AFK_SkeletonCube::atEnd(int gridDim) const
{
    return (coord.v[2] >= gridDim);
}


/* AFK_Skeleton implementation */

#define WITHIN_SKELETON_FLAG_GRID(x, y, z) \
    (((x) >= 0 && (x) < gridDim) && \
    ((y) >= 0 && (y) < gridDim) && \
    ((z) >= 0 && (z) < gridDim))

#define SKELETON_FLAG_XY(x, y) grid[(x)][(y)]
#define SKELETON_FLAG_Z(z) (1uLL << (z))

enum AFK_SkeletonFlag AFK_Skeleton::testFlag(const AFK_SkeletonCube& where) const
{
    if (WITHIN_SKELETON_FLAG_GRID(where.coord.v[0], where.coord.v[1], where.coord.v[2]))
        return (SKELETON_FLAG_XY(where.coord.v[0], where.coord.v[1]) & SKELETON_FLAG_Z(where.coord.v[2])) ?
            AFK_SKF_SET : AFK_SKF_CLEAR;
    else
        return AFK_SKF_OUTSIDE_GRID;
}

void AFK_Skeleton::setFlag(const AFK_SkeletonCube& where)
{
    if (WITHIN_SKELETON_FLAG_GRID(where.coord.v[0], where.coord.v[1], where.coord.v[2]))
        SKELETON_FLAG_XY(where.coord.v[0], where.coord.v[1]) |= SKELETON_FLAG_Z(where.coord.v[2]);
}

void AFK_Skeleton::clearFlag(const AFK_SkeletonCube& where)
{
    if (WITHIN_SKELETON_FLAG_GRID(where.coord.v[0], where.coord.v[1], where.coord.v[2]))
        SKELETON_FLAG_XY(where.coord.v[0], where.coord.v[1]) &= ~SKELETON_FLAG_Z(where.coord.v[2]);
}

int AFK_Skeleton::grow(
    const AFK_SkeletonCube& cube,
    int& bonesLeft,
    AFK_RNG& rng,
    const AFK_ShapeSizes& sSizes)
{
    int bones = 0;

    /* Flag this cube. */
    setFlag(cube);
    --bonesLeft;
    ++bones;

    for (int face = 0; face < 6; ++face)
    {
        if (bonesLeft == 0) break;

        AFK_SkeletonCube adjCube = cube.adjacentCube(face);
        if (testFlag(adjCube) == AFK_SKF_CLEAR &&
            rng.frand() < sSizes.skeletonBushiness)
        {
            /* Grow to this face. */
            bones += grow(adjCube, bonesLeft, rng, sSizes);
        }
    }

    return bones;
}

int AFK_Skeleton::embellish(
    const AFK_Skeleton& upper,
    const Vec3<int64_t>& upperOffset,
    AFK_RNG& rng,
    int subdivisionFactor,
    float bushiness)
{
    int bones = 0;

    /* TODO: This stuff isn't correct right now, because the cell subdivision
     * doesn't correctly track the possible skeleton embellishments.
     * I'm going to temporarily suppress it (by forcing `bushiness' to 0)
     * while I work on other bits.
     */
    float embBushiness = /* bushiness */ 0.0f;

    /* There are lots of ways I could do this.
     * As a relatively simple first approach that _should_ produce
     * good results (without needing insane levels of cross-
     * referencing), I'm going to try:
     */

    /* (1) Fill out a randomly morphed skeleton that sort of matches
     * the upper one.
     */
    for (AFK_SkeletonCube cube = AFK_SkeletonCube(); !cube.atEnd(gridDim); cube.advance(gridDim))
    {
        AFK_SkeletonCube upperCube = cube.upperCube(upperOffset, subdivisionFactor);

        /* Calculate the upper cube's adjacency number (0 for none, to 6 for everywhere) */
        int upperAdjacencyNumber = 0;
        for (int face = 0; face < 6; ++face)
        {
            AFK_SkeletonCube adjCube = upperCube.adjacentCube(face);
            if (upper.testFlag(adjCube) == AFK_SKF_SET) ++upperAdjacencyNumber;
        }

        if (upper.testFlag(upperCube) == AFK_SKF_SET)
        {
            /* If the upper cube has maximum adjacency I must always
             * set the flag in the lower cube: I don't want to introduce
             * invisible holes in the middle of shapes.
             * Otherwise, I'll merely *probably* set the flag in the
             * lower cube.
             */
            if ((upperAdjacencyNumber == 6) || (rng.frand() >= embBushiness))
            {
                setFlag(cube);
                ++bones;
            }
        }
        else
        {
            /* If it's got adjacency, I've got a regular chance of
             * setting it.
             */
            if ((upperAdjacencyNumber > 0) && (rng.frand() < embBushiness))
            {
                setFlag(cube);
                ++bones;
            }
        }
    }

    /* (2) Go around checking adjacency and plugging gaps.  I don't
     * want to introduce too many holes!
     */
    for (AFK_SkeletonCube cube = AFK_SkeletonCube(upperOffset);
        !cube.atEnd(gridDim); cube.advance(gridDim))
    {
        float chanceToNotPlug = 1.0f;
        for (int face = 0; face < 6; ++face)
        {
            AFK_SkeletonCube adjCube = cube.adjacentCube(face);
            if (testFlag(adjCube) == AFK_SKF_SET)
                chanceToNotPlug *= (1.0f - embBushiness);
        }

        if (rng.frand() >= chanceToNotPlug)
        {
            setFlag(cube);
            ++bones;
        }
    }

    return bones;
}

AFK_Skeleton::AFK_Skeleton():
    boneCount(0)
{
    memset(grid, 0, sizeof(uint64_t) * gridDim * gridDim);
}

int AFK_Skeleton::make(AFK_RNG& rng, const AFK_ShapeSizes& sSizes)
{
    /* Begin in the middle. */
    int skeletonSize = (int)sSizes.skeletonMaxSize;
    boneCount = grow(AFK_SkeletonCube(afk_vec3<int64_t>(gridDim / 2, gridDim / 2, gridDim / 2)),
        skeletonSize,
        rng,
        sSizes);
    return boneCount;
}

int AFK_Skeleton::make(
    const AFK_Skeleton& upper,
    const Vec3<int64_t>& upperOffset,
    AFK_RNG& rng,
    const AFK_ShapeSizes& sSizes)
{
    if (upper.getBoneCount() > 0)
    {
        /* Try embellishing that upper skeleton. */
        boneCount = embellish(upper, upperOffset, rng, sSizes.subdivisionFactor, sSizes.skeletonBushiness);
    }
    else
    {
        boneCount = 0;
    }

    return boneCount;
}

int AFK_Skeleton::getBoneCount(void) const
{
    return boneCount;
}

bool AFK_Skeleton::within(const AFK_SkeletonCube& cube) const
{
    return (testFlag(cube) == AFK_SKF_SET);
}

int AFK_Skeleton::getAdjacency(const AFK_SkeletonCube& cube) const
{
    int adj = 0;
    for (int face = 0; face < 6; ++face)
    {
        if (testFlag(cube.adjacentCube(face)) == AFK_SKF_SET)
            adj |= (1<<face);
    }

    return adj;
}

int AFK_Skeleton::getFullAdjacency(const AFK_SkeletonCube& cube) const
{
    int adj = 0;
    for (int64_t x = -1; x <= 1; ++x)
    {
        for (int64_t y = -1; y <= 1; ++y)
        {
            for (int64_t z = -1; z <= 1; ++z)
            {
                AFK_SkeletonCube adjCube(cube.coord + afk_vec3<int64_t>(x, y, z));
                if (testFlag(adjCube) == AFK_SKF_SET)
                    adj |= (1<<((x+1)*9 + (y+1)*3 + (z+1)));
            }
        }
    }

    return adj;
}

int AFK_Skeleton::getCoAdjacency(const AFK_SkeletonCube& cube) const
{
    int adj = 0;
    for (int64_t x = -1; x <= 1; ++x)
    {
        for (int64_t y = -1; y <= 1; ++y)
        {
            for (int64_t z = -1; z <= 1; ++z)
            {
                AFK_SkeletonCube adjCube(cube.coord + afk_vec3<int64_t>(x, y, z));
                if (testFlag(adjCube) == AFK_SKF_SET &&
                    (x == 0 || testFlag(AFK_SkeletonCube(cube.coord + afk_vec3<int64_t>(0, y, z))) == AFK_SKF_SET) &&
                    (y == 0 || testFlag(AFK_SkeletonCube(cube.coord + afk_vec3<int64_t>(x, 0, z))) == AFK_SKF_SET) &&
                    (z == 0 || testFlag(AFK_SkeletonCube(cube.coord + afk_vec3<int64_t>(x, y, 0))) == AFK_SKF_SET) &&
                    ((x == 0 && y == 0) || testFlag(AFK_SkeletonCube(cube.coord + afk_vec3<int64_t>(0, 0, z))) == AFK_SKF_SET) &&
                    ((x == 0 && z == 0) || testFlag(AFK_SkeletonCube(cube.coord + afk_vec3<int64_t>(0, y, 0))) == AFK_SKF_SET) &&
                    ((y == 0 && z == 0) || testFlag(AFK_SkeletonCube(cube.coord + afk_vec3<int64_t>(x, 0, 0))) == AFK_SKF_SET))
                {
                    adj |= (1<<((x+1)*9 + (y+1)*3 + (z+1)));
                }
            }
        }
    }

    return adj;
}

AFK_Skeleton::Bones::Bones(const AFK_Skeleton& _skeleton):
    skeleton(_skeleton)
{
    /* I start `x' off at -1 here so that it will advance to,
     * and test, the first field when next() is first called.
     */
    thisBone = AFK_SkeletonCube(afk_vec3<int64_t>(-1LL, 0LL, 0LL));
}

bool AFK_Skeleton::Bones::hasNext(void)
{
    /* Shortcut.
     * TODO: I can also make one involving checking entire
     * grid words for zero (there will be plenty like this).
     */
    if (skeleton.boneCount == 0) return false;

    nextBone = thisBone;
    do
    {   
        nextBone.advance(skeleton.gridDim);
        if (nextBone.atEnd(skeleton.gridDim)) return false;
    } while (skeleton.testFlag(nextBone) != AFK_SKF_SET);

    return true;
}

AFK_SkeletonCube AFK_Skeleton::Bones::next(void)
{
    if (!hasNext()) throw AFK_Exception("Skeleton ran out of bones");
    thisBone = nextBone;

    return thisBone;
}

