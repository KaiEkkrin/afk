/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cfloat>
#include <sstream>

#include "core.hpp"
#include "debug.hpp"
#include "displayed_landscape_tile.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "rng/boost_taus88.hpp"


/* AFK_LandscapeGeometry implementation */

AFK_LandscapeGeometry::AFK_LandscapeGeometry(
    size_t vCount, size_t iCount,
    AFK_GLBufferQueue *vSource, AFK_GLBufferQueue *iSource):
    vs(vCount, vSource), is(iCount, iSource)
{
}


void afk_getLandscapeSizes(
    unsigned int pointSubdivisionFactor,
    unsigned int& o_landscapeTileVsSize,
    unsigned int& o_landscapeTileIsSize)
{
#if AFK_LANDSCAPE_TYPE == AFK_LANDSCAPE_TYPE_SMOOTH
    unsigned int landscapeTileVCount = SQUARE(pointSubdivisionFactor + 2);
#elif AFK_LANDSCAPE_TYPE == AFK_LANDSCAPE_TYPE_FLAT
    unsigned int landscapeTileVCount = SQUARE(pointSubdivisionFactor + 1);
#else
#error "Unrecognised AFK landscape type"
#endif
    
    o_landscapeTileVsSize = landscapeTileVCount * sizeof(struct AFK_VcolPhongVertex);
    o_landscapeTileIsSize = landscapeTileVCount * sizeof(Vec3<unsigned int>);
}

/* AFK_LandscapeTile implementation */

void AFK_LandscapeTile::computeFlatTriangle(
    const Vec3<float> *vertices,
    const Vec3<float> *colours,
    const Vec3<unsigned int>& indices,
    unsigned int triangleVOff)
{
    unsigned int i;

    Vec3<float> crossP = ((vertices[indices.v[1]] - vertices[indices.v[0]]).cross(
        vertices[indices.v[2]] - vertices[indices.v[0]]));

    /* Try to sort out the winding order so that under GL_CCW,
     * all triangles are facing upwards
     */
    Vec3<unsigned int> triangle;
    triangle.v[0] = triangleVOff;
    if (crossP.v[1] >= 0.0f)
    {
        triangle.v[1] = triangleVOff + 2;
        triangle.v[2] = triangleVOff + 1;
    }   
    else
    {
        triangle.v[1] = triangleVOff + 1;
        triangle.v[2] = triangleVOff + 2;
    }

    geometry->is.t.push_back(triangle);

    /* I always want to compute the flat normal here.
     * The correct normal for smooth triangles is the
     * sum of these for all the triangles at a vertex.
     */
    Vec3<float> normal = crossP /* .normalise() */; /* TODO I normalise in the FS -- do I need it here too? */

    for (i = 0; i < 3; ++i)
    {
        struct AFK_VcolPhongVertex vertex;
        vertex.location = vertices[indices.v[i]];
        vertex.colour = colours[indices.v[i]];
        vertex.normal = normal;
        geometry->vs.t.push_back(vertex);
    }
}

void AFK_LandscapeTile::computeSmoothTriangle(
    const Vec3<unsigned int>& indices,
    bool emitIndices)
{
    Vec3<float> crossP =
        ((rawVertices[indices.v[1]] - rawVertices[indices.v[0]]).cross(
        rawVertices[indices.v[2]] - rawVertices[indices.v[0]]));

    Vec3<float> normal = crossP /* .normalise() */; /* TODO I normalise in the FS -- do I need it here too? */

    /* Update the first triangle with the normal and colour. */
    geometry->vs.t[indices.v[0]].colour += rawColours[indices.v[1]];
    geometry->vs.t[indices.v[0]].colour += rawColours[indices.v[2]];
    geometry->vs.t[indices.v[0]].normal += normal;

    /* Emit indices if requested */
    if (emitIndices)
    {
        /* This winding order reasoning is the same as in
         * computeFlatTriangle
         */
        if (crossP.v[1] >= 0.0f)
        {
            geometry->is.t.push_back(afk_vec3<unsigned int>(
                indices.v[0], indices.v[2], indices.v[1]));
        }
        else
        {
            geometry->is.t.push_back(indices);
        }
    }
}

void AFK_LandscapeTile::vertices2FlatTriangles(
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor,
    AFK_GLBufferQueue *vSource,
    AFK_GLBufferQueue *iSource)
{
    /* Each vertex generates 2 triangles (i.e. 6 triangle vertices) when
     * combined with the 3 vertices adjacent to it.
     */
    size_t expectedVertexCount = SQUARE(pointSubdivisionFactor + 1) * 6;

    /* Sanity check */
    if (geometry)
    {
        throw AFK_Exception("Called vertices2FlatTriangles with pre-existing computed vertices");
    }

    /* Set things up */
    geometry = new AFK_LandscapeGeometry(expectedVertexCount, expectedVertexCount, vSource, iSource);

    /* To make the triangles, I chew one row and the next at once.
     * Each triangle pair is:
     * ((row2, col1), (row1, col1), (row1, col2)),
     * ((row2, col1), (row1, col2), (row2, col2)).
     * The grid given to me goes from 0 to pointSubdivisionFactor+1
     * in both directions, so as to join up with adjacent cells.
     */
    unsigned int triangleVOff = 0;
    for (unsigned int row = 0; row < pointSubdivisionFactor; ++row)
    {
        for (unsigned int col = 0; col < pointSubdivisionFactor; ++col)
        {
            unsigned int i_r1c1 = row * (pointSubdivisionFactor+1) + col;
            unsigned int i_r1c2 = row * (pointSubdivisionFactor+1) + (col + 1);
            unsigned int i_r2c1 = (row + 1) * (pointSubdivisionFactor+1) + col;
            unsigned int i_r2c2 = (row + 1) * (pointSubdivisionFactor+1) + (col + 1);

            /* The location of the vertex at the first row governs
             * which cell the triangle belongs to.
             */

            Vec3<unsigned int> indices1 = afk_vec3<unsigned int>(i_r2c1, i_r1c1, i_r1c2);
            computeFlatTriangle(
                rawVertices, rawColours, indices1, triangleVOff);

            triangleVOff += 3;

            Vec3<unsigned int> indices2 = afk_vec3<unsigned int>(i_r2c1, i_r1c2, i_r2c2);
            computeFlatTriangle(
                rawVertices, rawColours, indices2, triangleVOff);

            triangleVOff += 3;

            /* Update the bounds */
            if (yBoundLower > rawVertices[i_r1c1].v[1]) yBoundLower = rawVertices[i_r1c1].v[1];
            if (yBoundUpper < rawVertices[i_r1c1].v[1]) yBoundUpper = rawVertices[i_r1c1].v[1];
        }
    }
}

void AFK_LandscapeTile::vertices2SmoothTriangles(
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor,
    AFK_GLBufferQueue *vSource,
    AFK_GLBufferQueue *iSource)
{
    /* Because I have excess on all 4 sides of the tile, this is
     * the real size of a dimension of the grid.
     */
    unsigned int dimSize = pointSubdivisionFactor + 2;

    /* Each vertex generates 2 triangles (like when I make flat
     * triangles), but this time there is no vertex amplification.
     */
    size_t expectedVertexCount = SQUARE(dimSize);
    size_t expectedIndexCount = SQUARE(dimSize) * 6;

    /* Sanity check */
    if (geometry)
    {
        throw AFK_Exception("Called vertices2SmoothTriangles with pre-existing computed vertices");
    }

    /* Set things up */
    geometry = new AFK_LandscapeGeometry(expectedVertexCount, expectedIndexCount, vSource, iSource);

    /* In this case, I need to zero the colours and normals,
     * because I'm going to be accumulating into them.  I can
     * also copy the locations into the vertex array, so that
     * computeSmoothTriangle() doesn't have to think about
     * that.
     */
    for (unsigned int row = 0; row < dimSize; ++row)
    {
        for (unsigned int col = 0; col < dimSize; ++col)
        {
            struct AFK_VcolPhongVertex v;
            v.location = rawVertices[row * dimSize + col];
            v.colour = rawColours[row * dimSize + col];
            v.normal = afk_vec3<float>(0.0f, 0.0f, 0.0f);
            geometry->vs.t.push_back(v);

            /* Update the bounds */
            if (yBoundLower > rawVertices[row * dimSize + col].v[1]) yBoundLower = rawVertices[row * dimSize + col].v[1];
            if (yBoundUpper < rawVertices[row * dimSize + col].v[1]) yBoundUpper = rawVertices[row * dimSize + col].v[1];
        }
    }

    /* Now, I need to iterate through my new vertex array,
     * contribute the triangles to the index buffer, and
     * accumulate the colours and normals.
     * Like in vertices2FlatTriangles, I chew one row and
     * the next at once.
     */
    for (unsigned int row = 0; row < (pointSubdivisionFactor + 1); ++row)
    {
        for (unsigned int col = 0; col < (pointSubdivisionFactor + 1); ++col)
        {
            unsigned int i_r1c1 = row * (dimSize) + col;
            unsigned int i_r1c2 = row * (dimSize) + (col + 1);
            unsigned int i_r2c1 = (row + 1) * (dimSize) + col;
            unsigned int i_r2c2 = (row + 1) * (dimSize) + (col + 1);
            
            /* I don't want to emit the triangles if I'm on the
             * lower x or z edges: those are for adjacent tiles,
             * not mine.  I still want to accumulate colour
             * and normal though.
             */
            Vec3<unsigned int> indices1 = afk_vec3<unsigned int>(i_r2c1, i_r1c1, i_r1c2);
            computeSmoothTriangle(indices1, (row > 0 && col > 0));

            Vec3<unsigned int> indices2 = afk_vec3<unsigned int>(i_r2c1, i_r1c2, i_r2c2);
            computeSmoothTriangle(indices2, (row > 0 && col > 0));
        }
    }
}

AFK_LandscapeTile::AFK_LandscapeTile():
    AFK_Claimable(),
    haveTerrainDescriptor(false),
    rawVertices(NULL),
    rawColours(NULL),
    rawVertexCount(0),
    geometry(NULL),
    yBoundLower(FLT_MAX),
    yBoundUpper(-FLT_MAX)
{
    futureGeometry.store(NULL);
}

AFK_LandscapeTile::~AFK_LandscapeTile()
{
    if (futureGeometry.load()) delete futureGeometry.load();
    if (geometry) delete geometry;
    if (rawVertices) delete[] rawVertices;
    if (rawColours) delete[] rawColours;
}

bool AFK_LandscapeTile::hasTerrainDescriptor() const
{
    return haveTerrainDescriptor;
}

void AFK_LandscapeTile::makeTerrainDescriptor(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    const AFK_Tile& tile,
    float minCellSize,
    const Vec3<float> *forcedTint)
{
    if (!haveTerrainDescriptor)
    {
        /* TODO RNG in thread local storage so I don't have to re-make
         * ones on the stack?  (inefficient?)
         */
        AFK_Boost_Taus88_RNG rng;

        /* I'm going to make 5 terrain tiles. */
        AFK_Tile descriptorTiles[5];
        terrainDescriptor.reserve(5);
        tile.enumerateDescriptorTiles(&descriptorTiles[0], 5, subdivisionFactor);

        for (unsigned int i = 0; i < 5; ++i)
        {
            rng.seed(descriptorTiles[i].rngSeed());
            boost::shared_ptr<AFK_TerrainTile> t(new AFK_TerrainTile());
            Vec3<float> tileCoord = descriptorTiles[i].toWorldSpace(minCellSize);
            t->make(tileCoord,
                pointSubdivisionFactor * (descriptorTiles[i].coord.v[2] / tile.coord.v[2]),
                subdivisionFactor, minCellSize, rng, forcedTint);
            terrainDescriptor.push_back(t);
        }

        haveTerrainDescriptor = true;
    }
}

bool AFK_LandscapeTile::claimGeometryRights(void)
{
    AFK_LandscapeGeometry **newFG = new AFK_LandscapeGeometry*();
    AFK_LandscapeGeometry **expectedFG = NULL;

    bool gotIt = futureGeometry.compare_exchange_strong(expectedFG, newFG);
    if (!gotIt) delete newFG;
    return gotIt;
}

void AFK_LandscapeTile::buildTerrainList(
    unsigned int threadId,
    AFK_TerrainList& list,
    const AFK_Tile& tile,
    unsigned int subdivisionFactor,
    float maxDistance,
    const AFK_LANDSCAPE_CACHE *cache) const
{
    /* Add the local terrain tiles to the list. */
    for (std::vector<boost::shared_ptr<AFK_TerrainTile> >::const_iterator it = terrainDescriptor.begin();
        it != terrainDescriptor.end(); ++it)
    {
        list.add(*it);
    }

    /* If this isn't the top level cell... */
    if (tile.coord.v[2] < maxDistance)
    {
        /* Find the parent tile in the cache.
         * If it's not here I'll throw an exception -- that would be a bug.
         */
        AFK_Tile parentTile = tile.parent(subdivisionFactor);
        AFK_LandscapeTile& parentLandscapeTile = cache->at(parentTile);
        enum AFK_ClaimStatus claimStatus = parentLandscapeTile.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
        if (claimStatus != AFK_CL_CLAIMED_SHARED && claimStatus != AFK_CL_CLAIMED_UPGRADABLE)
        {
            std::ostringstream ss;
            ss << "Unable to claim tile at " << parentTile << ": got status " << claimStatus;
            throw AFK_Exception(ss.str());
        }
        parentLandscapeTile.buildTerrainList(threadId, list, parentTile, subdivisionFactor, maxDistance, cache);
        parentLandscapeTile.release(threadId, claimStatus);
    }
}

void AFK_LandscapeTile::makeRawTerrain(
    enum AFK_LandscapeType type,
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor,
    float minCellSize)
{
    Vec3<float> worldTileCoord = baseTile.toWorldSpace(minCellSize);

    /* We always need a bit of excess.
     * The flat landscape type needs excess around the +x and +z in
     * order to make the edge triangles.
     * The smooth landscape type needs excess around the -x and -z too,
     * in order to make the normals for the x=0 and z=0 vertices.
     */
    int excessNX = (type == AFK_LANDSCAPE_TYPE_SMOOTH ? -1 : 0);
    int excessPX = 1;
    int excessNZ = (type == AFK_LANDSCAPE_TYPE_SMOOTH ? -1 : 0);
    int excessPZ = 1;

    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    if (type == AFK_LANDSCAPE_TYPE_SMOOTH)
        rawVertexCount = SQUARE(pointSubdivisionFactor + 2);
    else
        rawVertexCount = SQUARE(pointSubdivisionFactor + 1);
    rawVertices = new Vec3<float>[rawVertexCount];
    rawColours = new Vec3<float>[rawVertexCount];

    /* Populate the vertex and colour arrays. */
    size_t rawIndex = 0;

    for (int xi = excessNX; xi < (int)pointSubdivisionFactor + excessPX; ++xi)
    {
        for (int zi = excessNZ; zi < (int)pointSubdivisionFactor + excessPZ; ++zi)
        {
            /* Here I'm going to enumerate geometry between (0,0)
             * and (1,1)...
             */
            float xf = (float)xi / (float)pointSubdivisionFactor;
            float zf = (float)zi / (float)pointSubdivisionFactor;

            /* Jitter the vertices inside the cell around a bit. 
             * Not the edge ones; that will cause a join-up
             * headache (especially at different levels of detail).
             * TODO Of course, I'm going to have a headache on a
             * transition between LoDs anyway, but I'm trying not
             * to think about that too hard right now :P
             */ 
            float xdisp = 0.0f, zdisp = 0.0f;
#if 0
            if (xi > 0 && xi <= pointSubdivisionFactor)
                xdisp = rng.frand() * worldCell->coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);

            if (zi > 0 && zi <= pointSubdivisionFactor)
                zdisp = rng.frand() * worldCell->coord.v[3] / ((float)pointSubdivisionFactor * 2.0f);
#endif

            /* ... and transform it into world space. */
            rawVertices[rawIndex] = afk_vec3<float>(xf + xdisp, 0.0f, zf + zdisp) * worldTileCoord.v[2] +
                afk_vec3<float>(worldTileCoord.v[0], 0.0f, worldTileCoord.v[1]);
            rawColours[rawIndex] = afk_vec3<float>(0.1f, 0.1f, 0.1f);

            ++rawIndex;
        }
    }
}

void AFK_LandscapeTile::computeGeometry(
    unsigned int pointSubdivisionFactor,
    const AFK_Tile& baseTile,
    const AFK_TerrainList& terrainList,
    AFK_GLBufferQueue *vSource,
    AFK_GLBufferQueue *iSource)
{
    if (!rawVertices || !rawColours) return;

    /* Apply the terrain transform.
     * TODO This may be slow -- obvious candidate for OpenCL?  * But, the cache may rescue me; profile first!
     */
    terrainList.compute(rawVertices, rawColours, rawVertexCount);

    /* I've completed my vertex array!  Now, compute the triangles
     * and choose the actual world cells that they ought to be
     * matched with
     */
    if (AFK_LANDSCAPE_TYPE == AFK_LANDSCAPE_TYPE_FLAT)
        vertices2FlatTriangles(baseTile, pointSubdivisionFactor, vSource, iSource);
    else
        vertices2SmoothTriangles(baseTile, pointSubdivisionFactor, vSource, iSource);

    /* We're done! */
    *(futureGeometry.load()) = geometry;

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;

    rawVertices = NULL;
    rawColours = NULL;
    rawVertexCount = 0;
}

bool AFK_LandscapeTile::hasGeometry() const
{
    return (futureGeometry.load() != NULL);
}

float AFK_LandscapeTile::getYBoundLower() const
{
    if (!hasGeometry()) throw AFK_Exception("Tried to call getYBoundLower() without geometry");
    return yBoundLower;
}

float AFK_LandscapeTile::getYBoundUpper() const
{
    if (!hasGeometry()) throw AFK_Exception("Tried to call getYBoundUpper() without geometry");
    return yBoundUpper;
}

AFK_DisplayedLandscapeTile *AFK_LandscapeTile::makeDisplayedLandscapeTile(const AFK_Cell& cell, float minCellSize)
{
    AFK_DisplayedLandscapeTile *dlt = NULL;
#if 1
    Vec4<float> realCoord = cell.toWorldSpace(minCellSize);
    float cellBoundLower = realCoord.v[1];
    float cellBoundUpper = realCoord.v[1] + realCoord.v[3];

    /* The `<=' operator here: someone needs to own the 0-plane.  I'm
     * going to declare it to be the cell above not the cell below.
     */
    if (cellBoundLower <= yBoundUpper && cellBoundUpper > yBoundLower)
        dlt = new AFK_DisplayedLandscapeTile(futureGeometry.load(), cellBoundLower, cellBoundUpper);
#else
    dlt = new AFK_DisplayedLandscapeTile(futureGeometry.load(), -32768.0, 32768.0);
#endif

    return dlt;
}

AFK_Frame AFK_LandscapeTile::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_LandscapeTile::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t)
{
    os << "Landscape tile";
    if (t.haveTerrainDescriptor) os << " (Terrain)";
    os << " (" << std::dec << t.rawVertexCount << "vertices)";
    if (t.geometry) os << " (Computed with bounds: " << t.yBoundLower << " - " << t.yBoundUpper << ")";
    return os;
}

