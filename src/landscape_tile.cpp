/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cfloat>
#include <sstream>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "rng/boost_taus88.hpp"


/* AFK_LandscapeTile implementation */

AFK_LandscapeTile::AFK_LandscapeTile():
    AFK_Claimable(),
    haveTerrainDescriptor(false),
    jigsaws(NULL),
    yBoundLower(-FLT_MAX),
    yBoundUpper(FLT_MAX)
{
}

AFK_LandscapeTile::~AFK_LandscapeTile()
{
}

bool AFK_LandscapeTile::hasTerrainDescriptor() const
{
    return haveTerrainDescriptor;
}

void AFK_LandscapeTile::makeTerrainDescriptor(
    const AFK_LandscapeSizes& lSizes,
    const AFK_Tile& tile,
    float minCellSize)
{
    if (!haveTerrainDescriptor)
    {
        /* TODO RNG in thread local storage so I don't have to re-make
         * ones on the stack?  (inefficient?)
         */
        AFK_Boost_Taus88_RNG rng;

        /* I'm going to make 5 terrain tiles. */
        AFK_Tile descriptorTiles[5];
        tile.enumerateDescriptorTiles(&descriptorTiles[0], 5, lSizes.subdivisionFactor);

        for (unsigned int i = 0; i < 5; ++i)
        {
            rng.seed(descriptorTiles[i].rngSeed());
            Vec3<float> tileCoord = descriptorTiles[i].toWorldSpace(minCellSize);
            AFK_TerrainTile t;
            t.make(
                terrainFeatures,
                tileCoord,
                lSizes,
                rng);
            terrainTiles.push_back(t);
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
    list.extend(terrainFeatures, terrainTiles);

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

AFK_JigsawPiece AFK_LandscapeTile::getJigsawPiece(unsigned int threadId, int minJigsaw, AFK_JigsawCollection *_jigsaws)
{
    if (artworkState() == AFK_LANDSCAPE_TILE_HAS_ARTWORK) throw AFK_Exception("Tried to overwrite a tile's artwork");
    jigsaws = _jigsaws;
    jigsawPiece = jigsaws->grab(threadId, minJigsaw, jigsawPieceTimestamp);
    return jigsawPiece;
}

enum AFK_LandscapeTileArtworkState AFK_LandscapeTile::artworkState() const
{
    if (!hasTerrainDescriptor() || jigsawPiece == AFK_JigsawPiece()) return AFK_LANDSCAPE_TILE_NO_PIECE_ASSIGNED;

    AFK_Frame rowTimestamp = jigsaws->getPuzzle(jigsawPiece)->getTimestamp(jigsawPiece.piece);
#if 0
    if (rowTimestamp != jigsawPieceTimestamp)
    {
        AFK_DEBUG_PRINTL("Tile " << terrainTiles[0].getTileCoord() << ": timestamp expired (Old piece: " << jigsawPiece << ")")
    }
#endif

    return (rowTimestamp == jigsawPieceTimestamp) ? AFK_LANDSCAPE_TILE_HAS_ARTWORK : AFK_LANDSCAPE_TILE_PIECE_SWEPT;
}

float AFK_LandscapeTile::getYBoundLower() const
{
    return yBoundLower;
}

float AFK_LandscapeTile::getYBoundUpper() const
{
    return yBoundUpper;
}

void AFK_LandscapeTile::setYBounds(float _yBoundLower, float _yBoundUpper)
{
    /* Convert these bounds into world space.
     * The native tile is the first one in the list
     */
    float tileScale = terrainTiles[0].getTileScale();
    yBoundLower = _yBoundLower * tileScale;
    yBoundUpper = _yBoundUpper * tileScale;

    /* Debugging here because it's a good indicator that the tile
     * has been computed now.
     */
#if 0
    AFK_DEBUG_PRINTL("Tile " << terrainTiles[0].getTileCoord() << ": new y-bounds appeared: " << yBoundLower << ", " << yBoundUpper)
#endif
}

bool AFK_LandscapeTile::realCellWithinYBounds(const Vec4<float>& coord) const
{
    float cellBoundLower = coord.v[1];
    float cellBoundUpper = coord.v[1] + coord.v[3];

    /* The `<=' operator here: someone needs to own the 0-plane.  I'm
     * going to declare it to be the cell above not the cell below.
     */
    return (cellBoundLower <= yBoundUpper && cellBoundUpper > yBoundLower);
}

bool AFK_LandscapeTile::makeDisplayUnit(
    const AFK_Cell& cell,
    float minCellSize,
    AFK_JigsawPiece& o_jigsawPiece,
    AFK_LandscapeDisplayUnit& o_unit) const
{
    Vec4<float> realCoord = cell.toWorldSpace(minCellSize);
    bool displayed = realCellWithinYBounds(realCoord);
    
    if (displayed)
    {
        o_jigsawPiece = jigsawPiece;
        o_unit = AFK_LandscapeDisplayUnit(
            realCoord,
            jigsaws->getPuzzle(jigsawPiece)->getTexCoordST(jigsawPiece),
            yBoundLower,
            yBoundUpper);
    }

    return displayed;
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
    if (t.artworkState() == AFK_LANDSCAPE_TILE_HAS_ARTWORK) os << " (Computed with bounds: " << t.yBoundLower << " - " << t.yBoundUpper << ")";
    return os;
}

