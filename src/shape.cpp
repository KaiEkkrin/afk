/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <cstring>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>

#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"
#include "rng/aes.hpp"
#include "shape.hpp"


/* Fixed transforms for the six faces of a base cube.
 * TODO -- move this.  I suspect it needs to go back into
 * 3d_edge_shape_base : the shape_3dedge kernel is producing
 * single dimension displacements for the six faces.
 */
static struct FaceTransforms
{
    /* These are, in order:
     * - bottom
     * - left
     * - front
     * - back
     * - right
     * - top
     */
    AFK_Object obj[6];
    Mat4<float> trans[6];

    FaceTransforms()
    {
        obj[1].adjustAttitude(AXIS_ROLL, -M_PI_2);
        obj[1].displace(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        obj[2].adjustAttitude(AXIS_PITCH, M_PI_2);
        obj[2].displace(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        obj[3].adjustAttitude(AXIS_PITCH, -M_PI_2);
        obj[3].displace(afk_vec3<float>(0.0f, 0.0f, 1.0f));
        obj[4].adjustAttitude(AXIS_ROLL, M_PI_2);
        obj[4].displace(afk_vec3<float>(1.0f, 0.0f, 0.0f));
        obj[5].adjustAttitude(AXIS_PITCH, M_PI);
        obj[5].displace(afk_vec3<float>(0.0f, 1.0f, 1.0f));

        for (int i = 0; i < 6; ++i)
            trans[i] = obj[i].getTransformation();
    }
} faceTransforms;


/* AFK_ShapeCube implementation */

AFK_ShapeCube::AFK_ShapeCube(
    const Vec4<float>& _location):
        location(_location),
        vapourJigsawPiece(), vapourJigsawPieceTimestamp(),
        edgeJigsawPiece(), edgeJigsawPieceTimestamp()
{
}

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCube& cube)
{
    os << "ShapeCube(location=" << cube.location;
    os << ", vapour piece=" << cube.vapourJigsawPiece << " (timestamp " << cube.vapourJigsawPieceTimestamp << ")";
    os << ", edge piece=" << cube.edgeJigsawPiece << " (timestamp " << cube.edgeJigsawPieceTimestamp << ")";
    os << ")";
    return os;
}

/* AFK_SkeletonFlagGrid implementation */

AFK_SkeletonFlagGrid::AFK_SkeletonFlagGrid(int _gridDim):
    gridDim(_gridDim)
{
    assert((unsigned long long)gridDim < (sizeof(unsigned long long) * 8));
    grid = new unsigned long long *[gridDim];
    for (int x = 0; x < gridDim; ++x)
    {
        grid[x] = new unsigned long long[gridDim];
        memset(grid[x], 0, sizeof(unsigned long long) * gridDim);
    }
}

AFK_SkeletonFlagGrid::~AFK_SkeletonFlagGrid()
{
    for (int x = 0; x < gridDim; ++x)
    {
        delete[] grid[x];
    }

    delete[] grid;
}

/* Skeleton flag grid co-ordinates are such that (0, 0, 0) is in the middle. */
#define WITHIN_SKELETON_FLAG_GRID(x, y, z) \
    (((x) >= -(gridDim / 2) && (x) < (gridDim / 2)) && \
    ((y) >= -(gridDim / 2) && (y) < (gridDim / 2)) && \
    ((z) >= -(gridDim / 2) && (z) < (gridDim / 2)))

#define SKELETON_FLAG_XY(x, y) grid[(x) + gridDim / 2][(y) + gridDim / 2]
#define SKELETON_FLAG_Z(z) (1uLL << ((z) + gridDim / 2))

enum AFK_SkeletonFlag AFK_SkeletonFlagGrid::testFlag(const Vec3<int>& cube) const
{
    if (WITHIN_SKELETON_FLAG_GRID(cube.v[0], cube.v[1], cube.v[2]))
        return (SKELETON_FLAG_XY(cube.v[0], cube.v[1]) & SKELETON_FLAG_Z(cube.v[2])) ?
            AFK_SKF_SET : AFK_SKF_CLEAR;
    else
        return AFK_SKF_OUTSIDE_GRID;
}

void AFK_SkeletonFlagGrid::setFlag(const Vec3<int>& cube)
{
    if (WITHIN_SKELETON_FLAG_GRID(cube.v[0], cube.v[1], cube.v[2]))
        SKELETON_FLAG_XY(cube.v[0], cube.v[1]) |= SKELETON_FLAG_Z(cube.v[2]);
}

void AFK_SkeletonFlagGrid::clearFlag(const Vec3<int>& cube)
{
    if (WITHIN_SKELETON_FLAG_GRID(cube.v[0], cube.v[1], cube.v[2]))
        SKELETON_FLAG_XY(cube.v[0], cube.v[1]) &= ~SKELETON_FLAG_Z(cube.v[2]);
}


/* AFK_Shape implementation */

static Vec3<int> adjacentCube(const Vec3<int>& cube, unsigned int face)
{
    Vec3<int> adj;

    switch (face)
    {
    case 0: /* bottom */
        adj = afk_vec3<int>(cube.v[0], cube.v[1] - 1, cube.v[2]);
        break;

    case 1: /* left */
        adj = afk_vec3<int>(cube.v[0] - 1, cube.v[1], cube.v[2]);
        break;

    case 2: /* front */
        adj = afk_vec3<int>(cube.v[0], cube.v[1], cube.v[2] - 1);
        break;

    case 3: /* back */
        adj = afk_vec3<int>(cube.v[0], cube.v[1], cube.v[2] + 1);
        break;

    case 4: /* right */
        adj = afk_vec3<int>(cube.v[0] + 1, cube.v[1], cube.v[2]);
        break;

    case 5: /* top */
        adj = afk_vec3<int>(cube.v[0], cube.v[1] + 1, cube.v[2]);
        break;

    default:
        throw AFK_Exception("Invalid face");
    }

    return adj;
}

void AFK_Shape::makeSkeleton(
    AFK_RNG& rng,
    const AFK_ShapeSizes& sSizes,
    Vec3<int> cube,
    unsigned int *cubesLeft,
    std::vector<Vec3<int> >& o_skeletonCubes)
{
    /* Update the cubes. */
    if (*cubesLeft == 0) return;
    if (cubeGrid->testFlag(cube) != AFK_SKF_CLEAR) return;
    cubeGrid->setFlag(cube);
    o_skeletonCubes.push_back(cube);
    --(*cubesLeft);

    /* Update the point cubes with any that aren't included
     * already.
     * A cube will touch all 27 of the possible point cubes that
     * include it and its surroundings.
     */
    /* TODO: A point grid scale of 1 is much too small to start
     * with and just produces random noise.  I think I should
     * make this configurable (and play until I've got some
     * good values going on).  8 is a reasonable starting
     * point.
     */
    unsigned int pointGridScale = 8;
    for (int pI = pointGrids.size() - 1; pI >= 0; --pI)
    {
        Vec3<int> scaledCube = cube * pointGridScale;

        for (int x = -1; x <= 1; ++x)
        {
            for (int y = -1; y <= 1; ++y)
            {
                for (int z = -1; z <= 1; ++z)
                {
                    Vec3<int> pointCube = scaledCube + afk_vec3<int>(x, y, z);
                    if (pointGrids[pI]->testFlag(pointCube) == AFK_SKF_CLEAR)
                    {
                        pointGrids[pI]->setFlag(pointCube);
                        skeletonPointCubes.push_back(afk_vec4<int>(pointCube, pointGridScale));
                    }
                }
            }
        }

        pointGridScale = pointGridScale * 2;
    }

    for (unsigned int face = 0; face < 6 && *cubesLeft; ++face)
    {
        Vec3<int> adj = adjacentCube(cube, face);
        if (rng.frand() < sSizes.skeletonBushiness)
        {
            makeSkeleton(rng, sSizes, adj, cubesLeft, o_skeletonCubes);
        }
    }
}

AFK_Shape::AFK_Shape():
    AFK_Claimable(),
    cubeGrid(NULL)
{
    /* This is naughty, but I really want an auto-create
     * here.
     */
    unsigned int shapeCellCacheBitness = afk_suggestCacheBitness(
        afk_core.config->shape_skeletonMaxSize * 4);
    shapeCellCache = new AFK_SHAPE_CELL_CACHE(
        shapeCellCacheBitness,
        4,
        AFK_HashCell());
}

AFK_Shape::~AFK_Shape()
{
    if (cubeGrid) delete cubeGrid;
    for (std::vector<AFK_SkeletonFlagGrid *>::iterator pointGridIt = pointGrids.begin();
        pointGridIt != pointGrids.end(); ++pointGridIt)
    {
        delete *pointGridIt;
    }

    delete shapeCellCache;
}

bool AFK_Shape::has3DDescriptor() const
{
    return have3DDescriptor;
}

void AFK_Shape::make3DDescriptor(
    unsigned int threadId,
    unsigned int shapeKey,
    const AFK_ShapeSizes& sSizes)
{
    if (!have3DDescriptor)
    {
        /* TODO Interestingly, this is the first time I've actually
         * appeared to need a "better" RNG than Taus88: Taus88
         * doesn't appear to give enough randomness when seeded
         * with such a small number as the shapeKey.
         * If I end up randomly making lots of new shapes the
         * time taken to make a new AES RNG here is going to
         * become a bottleneck.
         */
        AFK_AES_RNG rng;
        boost::hash<unsigned int> uiHash;
        rng.seed(uiHash(shapeKey));

        /* TODO: Planes of symmetry and all sorts, here.  Lots to
         * experiment with in the quest for nice objects.
         */

        /* Come up with a skeleton.
         * A skeleton is characterised by a grid of flags, each
         * bit saying whether or not there is a cube in that
         * location.
         */
        cubeGrid = new AFK_SkeletonFlagGrid((int)sSizes.skeletonFlagGridDim);

        /* ...And also by a set of grids that determine where 
         * vapour features will exist.
         * TODO fix fix -- number of point grids -- and starting to
         * be pretty sure that I'm going to need to not just throw
         * all the cubes into the CL but instead pick a sub-list that
         * is used by each face...  flibble
         * Although, I'm also pretty sure I don't need the finest-
         * detail ones.  I can compress what I'm doing a great deal
         * by changing the point grids to contain a few large
         * points (with scale between cube size and cube size/subdivision factor,
         * say).
         */
        int pointGridCount;
        for (pointGridCount = 1;
            (1 << (pointGridCount - 1)) < (int)(sSizes.pointSubdivisionFactor / 2);
            ++pointGridCount)
        {
            pointGrids.push_back(new AFK_SkeletonFlagGrid((1 << pointGridCount) + 1));
        }
    
        std::vector<Vec3<int> > skeletonCubes;
        Vec3<int> startingCube = afk_vec3<int>(0, 0, 0);
        unsigned int skeletonCubesLeft = rng.uirand() % sSizes.skeletonMaxSize;
        makeSkeleton(rng, sSizes, startingCube, &skeletonCubesLeft, skeletonCubes);

        /* Now, for each skeleton point cube, add its points...
         */
        /* TODO Having a go at doing this in inverse order to see
         * if the large cubes crop up in the CL first
         */
        //for (std::vector<Vec4<int> >::iterator pointCubeIt = skeletonPointCubes.begin();
        //    pointCubeIt != skeletonPointCubes.end(); ++pointCubeIt)
        for (std::vector<Vec4<int> >::reverse_iterator pointCubeIt = skeletonPointCubes.rbegin();
            pointCubeIt != skeletonPointCubes.rend(); ++pointCubeIt)
        {
            /* TODO Individually debugging the various LoDs. */
            //if (pointCubeIt->v[3] == 1) continue;
            /* TODO I'm not convinced there is actually anything AT the
             * higher levels of detail -- or not very much.  Investigate.
             */
            //AFK_DEBUG_PRINTL(shapeKey << ": Using point cube: " << *pointCubeIt)

            long long pointCubeScale = pointCubeIt->v[3] * SHAPE_CELL_MAX_DISTANCE;
            AFK_Cell cell = afk_cell(afk_vec4<long long>(
                pointCubeIt->v[0] * pointCubeScale,
                pointCubeIt->v[1] * pointCubeScale,
                pointCubeIt->v[2] * pointCubeScale,
                pointCubeScale));

            AFK_ShapeCell& shapeCell = (*shapeCellCache)[cell];
            shapeCell.bind(cell);
            AFK_ClaimStatus claimStatus = shapeCell.claim(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
            switch (claimStatus)
            {
            case AFK_CL_CLAIMED_UPGRADABLE:
                if (!shapeCell.hasVapourDescriptor())
                {
                    claimStatus = shapeCell.upgrade(threadId, claimStatus);
                    if (claimStatus != AFK_CL_CLAIMED) throw AFK_Exception("ShapeCell claim upgrade failed");

                    shapeCell.makeVapourDescriptor(shapeKey, sSizes);
                }
                break;

            default:
                /* This must have been sorted out already by someone or
                 * other.
                 * This is all temporary code anyway so, meh.
                 */
                break;
            }

            shapeCell.release(threadId, claimStatus);
        }

        have3DDescriptor = true;
    }
}

void AFK_Shape::enqueueComputeUnits(
    unsigned int threadId,
    const AFK_ShapeSizes& sSizes,
    AFK_JigsawCollection *vapourJigsaws,
    AFK_JigsawCollection *edgeJigsaws,
    AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair,
    AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair) const
{
    /* TODO make this enqueue work items that check visibility and
     * detail pitch instead
     * :/
     */
    for (std::vector<Vec4<int> >::const_iterator pointCubeIt = skeletonPointCubes.begin();
        pointCubeIt != skeletonPointCubes.end(); ++pointCubeIt)
    {
        long long pointCubeScale = pointCubeIt->v[3] * SHAPE_CELL_MAX_DISTANCE;
        AFK_Cell cell = afk_cell(afk_vec4<long long>(
            pointCubeIt->v[0] * pointCubeScale,
            pointCubeIt->v[1] * pointCubeScale,
            pointCubeIt->v[2] * pointCubeScale,
            pointCubeScale));

        AFK_3DList list;
    
        AFK_ShapeCell& shapeCell = shapeCellCache->at(cell);
        AFK_ClaimStatus claimStatus = shapeCell.claim(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
        switch (claimStatus)
        {
        case AFK_CL_CLAIMED_UPGRADABLE:
        case AFK_CL_CLAIMED_SHARED:

            /* For now I only need to recompute the vapour if I'm
             * also recomputing the edges
             * TODO *2: This all ought to be done under an exclusive
             * claim.  I'm just gratuitously writing it wrong right
             * now, because I don't have a multithreading problem
             * But I will have.
             */
            if (!shapeCell.hasEdges(edgeJigsaws))
            {
                if (!shapeCell.hasVapour(vapourJigsaws))
                {
                    shapeCell.build3DList(threadId, list, sSizes.subdivisionFactor, shapeCellCache);
                    shapeCell.enqueueVapourComputeUnit(
                        threadId,
                        list,
                        sSizes,
                        vapourJigsaws,
                        vapourComputeFair);
                }

                shapeCell.enqueueEdgeComputeUnit(
                    threadId,
                    vapourJigsaws,
                    edgeJigsaws,
                    edgeComputeFair);
            }

            break;

        default:
            throw AFK_Exception("Can't claim shape cell");
        }

        shapeCell.release(threadId, claimStatus);
    }
}

void AFK_Shape::enqueueDisplayUnits(
    const AFK_Object& object,
    const AFK_JigsawCollection *edgeJigsaws,
    const AFK_ShapeSizes& sSizes,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const
{
    /* TODO make this enqueue work items that check visibility and
     * detail pitch instead
     * :/
     */
    Mat4<float> transform = object.getTransformation();

    for (std::vector<Vec4<int> >::const_iterator pointCubeIt = skeletonPointCubes.begin();
        pointCubeIt != skeletonPointCubes.end(); ++pointCubeIt)
    {
        long long pointCubeScale = pointCubeIt->v[3] * SHAPE_CELL_MAX_DISTANCE;
        AFK_Cell cell = afk_cell(afk_vec4<long long>(
            pointCubeIt->v[0] * pointCubeScale,
            pointCubeIt->v[1] * pointCubeScale,
            pointCubeIt->v[2] * pointCubeScale,
            pointCubeScale));

        /* TODO claim claim claim this code is temporary can't be arsed */
        AFK_ShapeCell& shapeCell = shapeCellCache->at(cell);
        shapeCell.enqueueEdgeDisplayUnit(
            transform,
            edgeJigsaws,
            entityDisplayFair);
    }
}

AFK_Frame AFK_Shape::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_Shape::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape)
{
    os << "Shape";
    if (shape.have3DDescriptor) os << " (3D descriptor)";
    return os;
}

