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

    is->t.push_back(triangle);

    Vec3<float> normal = crossP.normalise();

    for (i = 0; i < 3; ++i)
    {
        struct AFK_VcolPhongVertex vertex;
        vertex.location = vertices[indices.v[i]];
        vertex.colour = colours[indices.v[i]];
        vertex.normal = normal;
        vs->t.push_back(vertex);
    }
}

void AFK_LandscapeTile::vertices2FlatTriangles(
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor)
{
    /* Each vertex generates 2 triangles (i.e. 6 triangle vertices) when
     * combined with the 3 vertices adjacent to it.
     */
    size_t expectedVertexCount = SQUARE(pointSubdivisionFactor) * 6;

    /* Sanity check */
    if (vs)
    {
        throw new AFK_Exception("Called vertices2FlatTriangles with pre-existing computed vertices");
    }

    /* Set things up */
    boost::shared_ptr<AFK_LANDSCAPE_VERTEX_BUF> newVs(new AFK_LANDSCAPE_VERTEX_BUF(expectedVertexCount));
    vs = newVs;
    boost::shared_ptr<AFK_LANDSCAPE_INDEX_BUF> newIs(new AFK_LANDSCAPE_INDEX_BUF(expectedVertexCount));
    is = newIs;

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

void AFK_LandscapeTile::makeRawTerrain(
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor,
    float minCellSize)
{
    Vec3<float> worldTileCoord = baseTile.toWorldSpace(minCellSize);

    /* I'm going to need to sample the edges of the next cell along
     * the +ve x and z too, in order to join up with it.
     */
    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    rawVertexCount = SQUARE(pointSubdivisionFactor + 1);
    rawVertices = new Vec3<float>[rawVertexCount];
    rawColours = new Vec3<float>[rawVertexCount];

    /* Populate the vertex and colour arrays. */
    size_t rawIndex = 0;

    for (unsigned int xi = 0; xi < pointSubdivisionFactor + 1; ++xi)
    {
        for (unsigned int zi = 0; zi < pointSubdivisionFactor + 1; ++zi)
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

AFK_LandscapeTile::AFK_LandscapeTile():
    AFK_Claimable(),
    hasTerrainDescriptor(false),
    rawVertices(NULL),
    rawColours(NULL),
    rawVertexCount(0),
    yBoundLower(FLT_MAX),
    yBoundUpper(-FLT_MAX)
{
}

AFK_LandscapeTile::~AFK_LandscapeTile()
{
    if (rawVertices) delete[] rawVertices;
    if (rawColours) delete[] rawColours;
}

void AFK_LandscapeTile::makeTerrainDescriptor(
    unsigned int pointSubdivisionFactor,
    unsigned int subdivisionFactor,
    const AFK_Tile& tile,
    float minCellSize,
    const Vec3<float> *forcedTint)
{
    if (!hasTerrainDescriptor)
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

        hasTerrainDescriptor = true;
    }
}

void AFK_LandscapeTile::buildTerrainList(
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
        cache->at(parentTile).buildTerrainList(list, parentTile, subdivisionFactor, maxDistance, cache);
    }
}

bool AFK_LandscapeTile::hasRawTerrain(
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor,
    float minCellSize)
{
    if (!rawVertices && !vs) makeRawTerrain(baseTile, pointSubdivisionFactor, minCellSize);
    return (rawVertices != NULL);
}

void AFK_LandscapeTile::computeGeometry(
    unsigned int pointSubdivisionFactor,
    const AFK_Tile& baseTile,
    const AFK_TerrainList& terrainList)
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
    vertices2FlatTriangles(baseTile, pointSubdivisionFactor);

    /* I don't need this any more... */
    delete[] rawVertices;
    delete[] rawColours;

    rawVertices = NULL;
    rawColours = NULL;
    rawVertexCount = 0;
}

bool AFK_LandscapeTile::hasGeometry() const
{
    return (vs != NULL);
}

AFK_DisplayedLandscapeTile *AFK_LandscapeTile::makeDisplayedLandscapeTile(const AFK_Cell& cell, float minCellSize) const
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
        dlt = new AFK_DisplayedLandscapeTile(vs, is, cellBoundLower, cellBoundUpper);
#else
    dlt = new AFK_DisplayedLandscapeTile(vs, is, -32768.0, 32768.0);
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
    if (t.hasTerrainDescriptor) os << " (Terrain)";
    os << " (" << std::dec << t.rawVertexCount << "vertices)";
    if (t.vs) os << " (Computed with bounds: " << t.yBoundLower << " - " << t.yBoundUpper << ")";
    return os;
}

