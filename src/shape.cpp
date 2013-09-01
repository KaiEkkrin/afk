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
    std::vector<Vec3<int> >& o_skeletonCubes,
    std::vector<Vec4<int> >& o_skeletonPointCubes)
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
    unsigned int pointGridScale = 1;
    for (int pI = pointGrids.size() - 1; pI >= 0; --pI)
    {
        Vec3<int> scaledCube = cube / pointGridScale;

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
                        o_skeletonPointCubes.push_back(afk_vec4<int>(pointCube, pointGridScale));
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
            makeSkeleton(rng, sSizes, adj, cubesLeft, o_skeletonCubes, o_skeletonPointCubes);
        }
    }
}

AFK_Shape::AFK_Shape():
    AFK_Claimable(),
    cubeGrid(NULL),
    have3DDescriptor(false),
    vapourJigsaws(NULL),
    edgeJigsaws(NULL)
{
}

AFK_Shape::~AFK_Shape()
{
    if (cubeGrid) delete cubeGrid;
    for (std::vector<AFK_SkeletonFlagGrid *>::iterator pointGridIt = pointGrids.begin();
        pointGridIt != pointGrids.end(); ++pointGridIt)
    {
        delete *pointGridIt;
    }
}

bool AFK_Shape::has3DDescriptor() const
{
    return have3DDescriptor;
}

void AFK_Shape::make3DDescriptor(
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
        std::vector<Vec4<int> > skeletonPointCubes;
        Vec3<int> startingCube = afk_vec3<int>(0, 0, 0);
        unsigned int skeletonCubesLeft = rng.uirand() % sSizes.skeletonMaxSize;
        makeSkeleton(rng, sSizes, startingCube, &skeletonCubesLeft, skeletonCubes, skeletonPointCubes);

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

            AFK_3DVapourCube cube;
            cube.make(
                vapourFeatures,
                afk_vec4<float>(
                    (float)pointCubeIt->v[0] * (float)pointCubeIt->v[3],
                    (float)pointCubeIt->v[1] * (float)pointCubeIt->v[3],
                    (float)pointCubeIt->v[2] * (float)pointCubeIt->v[3],
                    (float)pointCubeIt->v[3]),
                sSizes,
                rng);
            vapourCubes.push_back(cube);
        }

        /* ... And add all cubes to the cube list.
         * TODO This process is associating all points with all faces,
         * which is very wasteful.  If I turn out to be at all limited
         * in performance by shape creation, I should come up with a
         * better scheme.
         */
        for (std::vector<Vec3<int> >::iterator cubeIt = skeletonCubes.begin();
            cubeIt != skeletonCubes.end(); ++cubeIt)
        {
            Vec3<float> cube = afk_vec3<float>(
                (float)cubeIt->v[0], (float)cubeIt->v[1], (float)cubeIt->v[2]);
            /* TODO: With LoD -- that 1.0f scale number down there will change! */
            cubes.push_back(AFK_ShapeCube(afk_vec4<float>(cube, 1.0f)));
        }

        have3DDescriptor = true;
    }
}

void AFK_Shape::build3DList(
    AFK_3DList& list)
{
    list.extend(vapourFeatures, vapourCubes);
}

void AFK_Shape::getCubesForCompute(
    unsigned int threadId,
    int minVapourJigsaw,
    int minEdgeJigsaw,
    AFK_JigsawCollection *_vapourJigsaws,
    AFK_JigsawCollection *_edgeJigsaws,
    std::vector<AFK_ShapeCube>& o_cubes)
{
    /* Sanity checks */
    if (!vapourJigsaws) vapourJigsaws = _vapourJigsaws;
    else if (vapourJigsaws != _vapourJigsaws) throw AFK_Exception("AFK_Shape: Mismatched vapour jigsaw collections");

    if (!edgeJigsaws) edgeJigsaws = _edgeJigsaws;
    else if (edgeJigsaws != _edgeJigsaws) throw AFK_Exception("AFK_Shape: Mismatched edge jigsaw collections");

    /* Check whether my edge pieces are ok */
    bool edgeOK = true;
    for (std::vector<AFK_ShapeCube>::iterator cubeIt = cubes.begin();
        edgeOK && cubeIt != cubes.end(); ++cubeIt)
    {
        edgeOK &= (cubeIt->edgeJigsawPiece != AFK_JigsawPiece() &&
            cubeIt->edgeJigsawPieceTimestamp == edgeJigsaws->getPuzzle(cubeIt->edgeJigsawPiece)->getTimestamp(cubeIt->edgeJigsawPiece));
    }

    if (!edgeOK)
    {
        /* Check whether my vapour pieces are ok */
        bool vapourOK = true;
        for (std::vector<AFK_ShapeCube>::iterator cubeIt = cubes.begin();
            vapourOK && cubeIt != cubes.end(); ++cubeIt)
        {
            vapourOK &= (cubeIt->vapourJigsawPiece != AFK_JigsawPiece() &&
                cubeIt->vapourJigsawPieceTimestamp == vapourJigsaws->getPuzzle(cubeIt->vapourJigsawPiece)->getTimestamp(cubeIt->vapourJigsawPiece));
        }

        /* All pieces need to be in the same jigsaw, so that I can
         * cross-reference during the edge pass:
         */
        if (!vapourOK)
        {
            AFK_JigsawPiece vapourPieces[cubes.size()];
            AFK_Frame vapourTimestamps[cubes.size()];
            vapourJigsaws->grab(
                threadId,
                minVapourJigsaw,
                vapourPieces,
                vapourTimestamps,
                cubes.size());

            for (size_t c = 0; c < cubes.size(); ++c)
            {
                cubes[c].vapourJigsawPiece = vapourPieces[c];
                cubes[c].vapourJigsawPieceTimestamp = vapourTimestamps[c];
            }
        }

        /* Same for the edge pieces */
        AFK_JigsawPiece edgePieces[cubes.size()];
        AFK_Frame edgeTimestamps[cubes.size()];
        edgeJigsaws->grab(
            threadId,
            minEdgeJigsaw,
            edgePieces,
            edgeTimestamps,
            cubes.size());

        for (size_t c = 0; c < cubes.size(); ++c)
        {
            cubes[c].edgeJigsawPiece = edgePieces[c];
            cubes[c].edgeJigsawPieceTimestamp = edgeTimestamps[c];
            o_cubes.push_back(cubes[c]);
        }
    }
}

enum AFK_ShapeArtworkState AFK_Shape::artworkState() const
{
    if (!has3DDescriptor()) return AFK_SHAPE_NO_PIECE_ASSIGNED;

    /* Scan the cubes and check for status. */
    enum AFK_ShapeArtworkState status = AFK_SHAPE_HAS_ARTWORK;
    for (std::vector<AFK_ShapeCube>::const_iterator cubeIt = cubes.begin(); cubeIt != cubes.end(); ++cubeIt)
    {
        if (cubeIt->edgeJigsawPiece == AFK_JigsawPiece()) return AFK_SHAPE_NO_PIECE_ASSIGNED;
        if (cubeIt->edgeJigsawPieceTimestamp != edgeJigsaws->getPuzzle(cubeIt->edgeJigsawPiece)->getTimestamp(cubeIt->edgeJigsawPiece))
            status = AFK_SHAPE_PIECE_SWEPT;
    }
    return status;
}

void AFK_Shape::enqueueDisplayUnits(
    const AFK_Object& object,
    AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const
{
    boost::shared_ptr<AFK_EntityDisplayQueue> q = entityDisplayFair.getUpdateQueue(0);
    Mat4<float> objTransform = object.getTransformation();

    for (std::vector<AFK_ShapeCube>::const_iterator cubeIt = cubes.begin(); cubeIt != cubes.end(); ++cubeIt)
    {
        AFK_JigsawPiece edgeJigsawPiece = cubeIt->edgeJigsawPiece;
        q->add(AFK_EntityDisplayUnit(
            objTransform,
            edgeJigsaws->getPuzzle(edgeJigsawPiece)->getTexCoordST(edgeJigsawPiece)));
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
    if (shape.artworkState() == AFK_SHAPE_HAS_ARTWORK) os << " (Computed)";
    return os;
}

