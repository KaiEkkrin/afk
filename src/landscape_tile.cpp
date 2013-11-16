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

#include <cfloat>
#include <sstream>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "rng/boost_taus88.hpp"


#define AFK_DEBUG_LANDSCAPE_BUILD 0

#if AFK_DEBUG_LANDSCAPE_BUILD
#define AFK_DEBUG_PRINT_LANDSCAPE_BUILD(expr) AFK_DEBUG_PRINT(expr)
#define AFK_DEBUG_PRINTL_LANDSCAPE_BUILD(expr) AFK_DEBUG_PRINTL(expr)
#else
#define AFK_DEBUG_PRINT_LANDSCAPE_BUILD(expr)
#define AFK_DEBUG_PRINTL_LANDSCAPE_BUILD(expr)
#endif


/* AFK_LandscapeTile implementation */

AFK_LandscapeTile::AFK_LandscapeTile():
    haveTerrainDescriptor(false),
    yBoundLower(-FLT_MAX),
    yBoundUpper(FLT_MAX)
{
}

AFK_LandscapeTile::~AFK_LandscapeTile()
{
    evict();
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
        AFK_Boost_Taus88_RNG rng;

        AFK_Tile descriptorTiles[afk_terrainTilesPerTile];
        tile.enumerateDescriptorTiles(&descriptorTiles[0], afk_terrainTilesPerTile, lSizes.subdivisionFactor);

        auto featureIt = terrainFeatures.begin();
        for (unsigned int i = 0; i < afk_terrainTilesPerTile; ++i)
        {
            rng.seed(descriptorTiles[i].rngSeed());
            Vec3<float> tileCoord = descriptorTiles[i].toWorldSpace(minCellSize);
            terrainTiles[i].make<FeatureArray::iterator>(
                featureIt,
                tileCoord,
                lSizes,
                rng);
        }

        assert(featureIt == terrainFeatures.end());

        AFK_DEBUG_PRINTL_LANDSCAPE_BUILD("makeTerrainDescriptor(): generated terrain for " << tile << " (terrain tiles " << AFK_InnerDebug<TileArray>(&terrainTiles) << ")")

        haveTerrainDescriptor = true;
    }
}

void AFK_LandscapeTile::buildTerrainList(
    unsigned int threadId,
    AFK_TerrainList& list,
    const AFK_Tile& tile,
    unsigned int subdivisionFactor,
    float maxDistance,
    AFK_LANDSCAPE_CACHE *cache) const
{
    /* TODO remove debug
     * So, I'm getting never-before-seen pointers here, indicating
     * a bug.  I suspect the Claimable stuff of maybe double
     * copying (over use of the move constructor?), or deleting
     * early.  It might be necessary to play with a reference
     * count after all, and certainly I should put debug tracking
     * into Claimable to see what's up.
     * ... no, I don't think there are any double copies or early
     * deletes.  Simply, heap-allocated memory is turning up
     * wrong on different threads.  The next thing to try is to
     * not use that rapid heap allocation and instead embed a
     * std::array in each LandscapeTile ...
     */
    AFK_DEBUG_PRINTL_LANDSCAPE_BUILD("buildTerrainList(): adding local terrain for " << tile << " (terrain tiles " << AFK_InnerDebug<TileArray>(&terrainTiles) << ")")

    /* Add the local terrain tiles to the list. */
    list.extend<FeatureArray, TileArray>(terrainFeatures, terrainTiles);

    /* If this isn't the top level tile... */
    if (tile.coord.v[2] < maxDistance)
    {
        /* Find the parent tile in the cache.
         * If it's not here I'll throw an exception -- that would be a bug.
         */
        AFK_Tile parentTile = tile.parent(subdivisionFactor);

        AFK_DEBUG_PRINTL_LANDSCAPE_BUILD("buildTerrainList(): looking for terrain for " << parentTile)

        try
        {
            auto parentLandscapeTileClaim = cache->get(threadId, parentTile).claimable.claim(threadId, AFK_CL_LOOP);
            parentLandscapeTileClaim.getShared().buildTerrainList(threadId, list, parentTile, subdivisionFactor, maxDistance, cache);
        }
        catch (AFK_PolymerOutOfRange e)
        {
            AFK_DEBUG_PRINTL("buildTerrainList(): can't find terrain for " << parentTile)
            throw e;
        }
    }
}

AFK_JigsawPiece AFK_LandscapeTile::getJigsawPiece(unsigned int threadId, int minJigsaw, AFK_JigsawCollection *jigsaws)
{
    if (artworkState(jigsaws) == AFK_LANDSCAPE_TILE_HAS_ARTWORK) throw AFK_Exception("Tried to overwrite a tile's artwork");
    jigsaws->grab(minJigsaw, &jigsawPiece, &jigsawPieceTimestamp, 1);
    return jigsawPiece;
}

enum AFK_LandscapeTileArtworkState AFK_LandscapeTile::artworkState(
    AFK_JigsawCollection *jigsaws) const
{
    if (!hasTerrainDescriptor() || !jigsawPiece) return AFK_LANDSCAPE_TILE_NO_PIECE_ASSIGNED;

    AFK_Jigsaw *jigsaw = jigsaws->getPuzzle(jigsawPiece);
    AFK_Frame rowTimestamp = jigsaw->getTimestamp(jigsawPiece);
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
    if (haveTerrainDescriptor)
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
    AFK_JigsawCollection *jigsaws,
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

void AFK_LandscapeTile::evict(void)
{
    haveTerrainDescriptor = false;
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t)
{
    os << "Landscape tile with jigsaw piece " << t.jigsawPiece;
    if (t.haveTerrainDescriptor) os << " (Terrain)";
    os << " (With bounds: " << t.yBoundLower << " - " << t.yBoundUpper << ")";
    return os;
}

