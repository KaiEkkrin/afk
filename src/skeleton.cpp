/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cstring>

#include "skeleton.hpp"



/* AFK_SkeletonCube implementation */

AFK_SkeletonCube::AFK_SkeletonCube()
{
    coord = afk_vec3<long long>(0LL, 0LL, 0LL);
}

AFK_SkeletonCube::AFK_SkeletonCube(const Vec3<long long>& _coord):
    coord(_coord)
{
}

AFK_SkeletonCube AFK_SkeletonCube::adjacentCube(int face) const
{
    AFK_SkeletonCube adj;

    switch (face)
    {
    case 0: /* bottom */
        adj.coord = afk_vec3<long long>(coord.v[0], coord.v[1] - 1, coord.v[2]);
        break;

    case 1: /* left */
        adj.coord = afk_vec3<long long>(coord.v[0] - 1, coord.v[1], coord.v[2]);
        break;

    case 2: /* front */
        adj.coord = afk_vec3<long long>(coord.v[0], coord.v[1], coord.v[2] - 1);
        break;

    case 3: /* back */
        adj.coord = afk_vec3<long long>(coord.v[0], coord.v[1], coord.v[2] + 1);
        break;

    case 4: /* right */
        adj.coord = afk_vec3<long long>(coord.v[0] + 1, coord.v[1], coord.v[2]);
        break;

    case 5: /* top */
        adj.coord = afk_vec3<long long>(coord.v[0], coord.v[1] + 1, coord.v[2]);
        break;

    default:
        throw AFK_Exception("Invalid face");
    }

    return adj;
}

AFK_SkeletonCube AFK_SkeletonCube::upperCube(const Vec3<long long>& upperOffset, unsigned int subdivisionFactor) const
{
    return AFK_SkeletonCube(afk_vec3<long long>(
        coord.v[0] / subdivisionFactor,
        coord.v[1] / subdivisionFactor,
        coord.v[2] / subdivisionFactor) + upperOffset);
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

void AFK_Skeleton::initGrid(void)
{
    if (grid) throw AFK_Exception("Tried to re-init grid");

    grid = new unsigned long long*[gridDim];
    for (int x = 0; x < gridDim; ++x)
    {
        grid[x] = new unsigned long long[gridDim];
        memset(grid[x], 0, sizeof(unsigned long long) * gridDim);
    }
}

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

void AFK_Skeleton::grow(
    const AFK_SkeletonCube& cube,
    int& bonesLeft,
    AFK_RNG& rng,
    const AFK_ShapeSizes& sSizes)
{
    /* Flag this cube. */
    setFlag(cube);
    --bonesLeft;

    for (int face = 0; face < 6; ++face)
    {
        if (bonesLeft == 0) break;

        AFK_SkeletonCube adjCube = cube.adjacentCube(face);
        if (testFlag(adjCube) == AFK_SKF_CLEAR &&
            rng.frand() < sSizes.skeletonBushiness)
        {
            /* Grow to this face. */
            grow(adjCube, bonesLeft, rng, sSizes);
        }
    }
}

AFK_Skeleton::AFK_Skeleton():
    grid(NULL)
{
}

AFK_Skeleton::~AFK_Skeleton()
{
    if (grid)
    {
        for (int x = 0; x < gridDim; ++x)
        {
            delete[] grid[x];
        }

        delete[] grid;
    }
}

void AFK_Skeleton::make(AFK_RNG& rng, const AFK_ShapeSizes& sSizes)
{
    gridDim = sSizes.skeletonFlagGridDim;
    initGrid();

    /* Begin in the middle. */
    int skeletonSize = (int)sSizes.skeletonMaxSize;
    grow(AFK_SkeletonCube(afk_vec3<long long>(gridDim / 2, gridDim / 2, gridDim / 2)),
        skeletonSize,
        rng,
        sSizes);
}

void AFK_Skeleton::make(
    const AFK_Skeleton& upper,
    const Vec3<long long>& upperOffset,
    AFK_RNG& rng,
    const AFK_ShapeSizes& sSizes)
{
    gridDim = sSizes.skeletonFlagGridDim;
    initGrid();

    /* There are lots of ways I could do this.
     * As a relatively simple first approach that _should_ produce
     * good results (without needing insane levels of cross-
     * referencing), I'm going to try:
     */

    /* (1) Fill out a randomly morphed skeleton that sort of matches
     * the upper one.
     */
    for (AFK_SkeletonCube cube = AFK_SkeletonCube(upperOffset);
        !cube.atEnd(gridDim); cube.advance(gridDim))
    {
        AFK_SkeletonCube upperCube = cube.upperCube(upperOffset, sSizes.subdivisionFactor);
        if (upper.testFlag(upperCube) == AFK_SKF_SET)
        {
            /* I'll probably set the flag in the lower cube too. */
            if (rng.frand() >= sSizes.skeletonBushiness)
                setFlag(cube);
        }
        else
        {
            /* If it's got adjacency, I've got a regular chance of
             * setting it.
             */
            bool hasAdjacency = false;
            for (int face = 0; face < 6; ++face)
            {
                AFK_SkeletonCube adjCube = cube.adjacentCube(face);
                AFK_SkeletonCube adjUpperCube = adjCube.upperCube(upperOffset, sSizes.subdivisionFactor);
                if (upper.testFlag(adjUpperCube) == AFK_SKF_SET)
                {
                    hasAdjacency = true;
                    break;
                }
            }

            if (hasAdjacency)
            {
                if (rng.frand() < sSizes.skeletonBushiness)
                    setFlag(cube);
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
                chanceToNotPlug *= (1.0f - sSizes.skeletonBushiness);
        }

        if (rng.frand() >= chanceToNotPlug)
            setFlag(cube);
    }
}

AFK_Skeleton::Bones::Bones(const AFK_Skeleton& _skeleton):
    skeleton(_skeleton)
{
    /* I start `x' off at -1 here so that it will advance to,
     * and test, the first field when next() is first called.
     */
    thisBone = AFK_SkeletonCube(afk_vec3<long long>(-1LL, 0LL, 0LL));
}

bool AFK_Skeleton::Bones::hasNext(void)
{
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

