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
    Vec3<float> *_rawVertices, Vec3<float> *_rawColours, size_t _rawVertexCount,
    size_t vCount, size_t iCount,
    AFK_GLBufferQueue *vSource, AFK_GLBufferQueue *iSource):
    rawVertices(_rawVertices), rawColours(_rawColours), rawVertexCount(_rawVertexCount),
    vs(vCount, vSource), is(iCount, iSource)
{
}


void afk_getLandscapeSizes(
    unsigned int pointSubdivisionFactor,
    unsigned int& o_landscapeTileVCount,
    unsigned int& o_landscapeTileICount,
    unsigned int& o_landscapeTileVsSize,
    unsigned int& o_landscapeTileIsSize)
{
    o_landscapeTileVCount = SQUARE(pointSubdivisionFactor + 2);
    o_landscapeTileICount = SQUARE(pointSubdivisionFactor) * 2;
    
    o_landscapeTileVsSize = o_landscapeTileVCount * sizeof(struct AFK_VcolPhongVertex);
    o_landscapeTileIsSize = o_landscapeTileICount * sizeof(struct AFK_VcolPhongIndex);
}

/* AFK_LandscapeTile implementation */

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
    const AFK_Tile& baseTile,
    unsigned int pointSubdivisionFactor,
    float minCellSize)
{
    Vec3<float> worldTileCoord = baseTile.toWorldSpace(minCellSize);

    /* We always need a bit of excess.
     * The smooth landscape type needs excess around the -x and -z too,
     * in order to make the normals for the x=0 and z=0 vertices.
     */
    int excessNX = -1;
    int excessPX = 1;
    int excessNZ = -1;
    int excessPZ = 1;

    /* TODO This is thrashing the heap.  Try making a single
     * scratch place for this in thread-local storage
     */
    rawVertexCount = SQUARE(pointSubdivisionFactor + 2);
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

    /* The rest gets left to the triangle program... */
    unsigned int vCount, iCount, vSize, iSize;
    afk_getLandscapeSizes(pointSubdivisionFactor, vCount, iCount, vSize, iSize);

    geometry = new AFK_LandscapeGeometry(
        rawVertices, rawColours, rawVertexCount,
        vCount, iCount, vSource, iSource);

    /* We're done! */
    *(futureGeometry.load()) = geometry;
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
#if 0
    Vec4<float> realCoord = cell.toWorldSpace(minCellSize);
    float cellBoundLower = realCoord.v[1];
    float cellBoundUpper = realCoord.v[1] + realCoord.v[3];

    /* The `<=' operator here: someone needs to own the 0-plane.  I'm
     * going to declare it to be the cell above not the cell below.
     */
    /* TODO I'm going to need to deal with the y bounds.  :-( */
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

