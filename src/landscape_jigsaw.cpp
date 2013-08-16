/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cstring>
#include <sstream>

#include "core.hpp"
#include "landscape_jigsaw.hpp"


/* AFK_LandscapeJigsawMetadata implementation */

#define TILE_USAGE_ROWSIZE ((jigsawSize.v[1] >> 6) + 1)
#define TILE_USAGE_FIELD(uv) (tileUsage[(uv).v[0]][((uv).v[1] >> 6)])
#define TILE_USAGE_BIT(v) (1uLL << ((v) & ((1 << 6) - 1)))

bool AFK_LandscapeJigsawMetadata::testTileUsage(const Vec2<int>& uv)
{
    return (TILE_USAGE_FIELD(uv) & TILE_USAGE_BIT(uv.v[1]));
}

void AFK_LandscapeJigsawMetadata::setTileUsage(const Vec2<int>& uv, bool inUse)
{
    if (inUse)
        TILE_USAGE_FIELD(uv) |= TILE_USAGE_BIT(uv.v[1]);
    else
        TILE_USAGE_FIELD(uv) &= ~TILE_USAGE_BIT(uv.v[1]);
}

void AFK_LandscapeJigsawMetadata::clearRowUsage(const int row)
{
    memset(tileUsage[row], 0, TILE_USAGE_ROWSIZE * sizeof(unsigned long long));
}

AFK_LandscapeJigsawMetadata::AFK_LandscapeJigsawMetadata(
    const Vec2<int>& _jigsawSize,
    AFK_LANDSCAPE_CACHE *_landscapeCache):
        jigsawSize(_jigsawSize),
        landscapeCache(_landscapeCache)
{
    tiles = new AFK_Tile *[jigsawSize.v[0]];
    tileUsage = new unsigned long long *[jigsawSize.v[0]];
    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        tiles[row] = new AFK_Tile[jigsawSize.v[1]];
        tileUsage[row] = new unsigned long long[TILE_USAGE_ROWSIZE];
        clearRowUsage(row);
    }
}

AFK_LandscapeJigsawMetadata::~AFK_LandscapeJigsawMetadata()
{
    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        delete[] tiles[row];
        delete[] tileUsage[row];
    }
    delete[] tiles;
    delete[] tileUsage;
}

void AFK_LandscapeJigsawMetadata::assign(const Vec2<int>& uv, const AFK_Tile& tile)
{
    if (testTileUsage(uv))
    {
        std::ostringstream ss;
        ss << "AFK_LandscapeJigsawMetadata: UV " << uv << " already assigned to " << tiles[uv.v[0]][uv.v[1]];
        throw AFK_Exception(ss.str());
    }

    tiles[uv.v[0]][uv.v[1]] = tile;  
    setTileUsage(uv, true);
}

bool AFK_LandscapeJigsawMetadata::canEvict(const AFK_Frame& lastSeen, const AFK_Frame& currentFrame)
{
    /* We can evict a landscape slot if it's more than 10 frames old */
    /* TODO Re-allow this after I've tested the multiple jigsaws thing. */
    //return (lastSeen.getNever() || (currentFrame - lastSeen) > 10);
    return (lastSeen.getNever());
}

void AFK_LandscapeJigsawMetadata::evicted(const int row)
{
    for (int column = 0; column < jigsawSize.v[1]; ++column)
    {
        if (testTileUsage(afk_vec2<int>(row, column)))
            landscapeCache->erase(tiles[row][column]);
    }

    clearRowUsage(row);
}


/* Factory function */

boost::shared_ptr<AFK_JigsawMetadata> afk_newLandscapeJigsawMeta(const Vec2<int>& _jigsawSize)
{
    boost::shared_ptr<AFK_JigsawMetadata> m(new AFK_LandscapeJigsawMetadata(
        _jigsawSize, afk_core.world->landscapeCache));
    return m;
}

