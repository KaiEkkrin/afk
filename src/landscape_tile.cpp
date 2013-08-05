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


/* AFK_LandscapeTile implementation */

AFK_LandscapeTile::AFK_LandscapeTile():
    AFK_Claimable(),
    haveTerrainDescriptor(false),
    jigsaws(NULL);
    yBoundLower(FLT_MAX),
    yBoundUpper(-FLT_MAX)
{
}

AFK_LandscapeTile::~AFK_LandscapeTile()
{
    if (jigsaws && jigsawPiece != AFK_JigsawPiece())
        jigsaws->replace(jigsawPiece);
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

AFK_JigsawPiece AFK_LandscapeTile::getJigsawPiece(AFK_JigsawCollection *_jigsaws)
{
    if (hasArtwork()) throw AFK_Exception("Tried to overwrite a tile's artwork");
    jigsaws = _jigsaws;
    jigsawPiece = jigsaws->grab();
    return jigsawPiece;
}

bool AFK_LandscapeTile::hasArtwork() const
{
    return (jigsawPiece != AFK_JigsawPiece());
}

float AFK_LandscapeTile::getYBoundLower() const
{
    return yBoundLower;
}

float AFK_LandscapeTile::getYBoundUpper() const
{
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
        dlt = new AFK_DisplayedLandscapeTile(cell.toWorldSpace(minCellSize), jigsawPiece, cellBoundLower, cellBoundUpper);
#else
    dlt = new AFK_DisplayedLandscapeTile(cell.toWorldSpace(minCellSize), jigsawPiece, -32768.0, 32768.0);
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
    if (t.hasArtwork()) os << " (Computed with bounds: " << t.yBoundLower << " - " << t.yBoundUpper << ")";
    return os;
}

