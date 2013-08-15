/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_JIGSAW_H_
#define _AFK_LANDSCAPE_JIGSAW_H_

#include "afk.hpp"

#include <boost/shared_ptr.hpp>

#include "data/frame.hpp"
#include "data/polymer_cache.hpp"
#include "def.hpp"
#include "jigsaw.hpp"
#include "landscape_tile.hpp"
#include "tile.hpp"

#ifndef AFK_LANDSCAPE_CACHE
#define AFK_LANDSCAPE_CACHE AFK_PolymerCache<AFK_Tile, AFK_LandscapeTile, AFK_HashTile>
#endif

/* Jigsaw specialisations for landscape data. */

class AFK_LandscapeJigsawMetadata: public AFK_JigsawMetadata
{
protected:
    const Vec2<int> jigsawSize;

    /* This is a 2D grid of the tiles in the landscape
     * cache, in the same arrangement as the jigsaw itself.
     */
    AFK_Tile **tiles;

    /* This tracks whether the tiles are in use.
     * Each entry is a bit field for 64 consecutive entries in
     * the cache, flagging whether a tile is set or not.
     */
    unsigned long long **tileUsage;

    /* For accessing the tile usage entries. */
    bool testTileUsage(const Vec2<int>& uv);
    void setTileUsage(const Vec2<int>& uv, bool inUse);
    void clearRowUsage(const int row);

    /* This is a reference to the landscape cache (which we don't own) */
    AFK_LANDSCAPE_CACHE *landscapeCache;

public:
    AFK_LandscapeJigsawMetadata(
        const Vec2<int>& _jigsawSize,
        AFK_LANDSCAPE_CACHE *_landscapeCache);
    virtual ~AFK_LandscapeJigsawMetadata();

    void assign(const Vec2<int>& uv, const AFK_Tile& tile);

    /* AFK_JigsawMetadata implementation. */
    virtual bool canEvict(const AFK_Frame& lastSeen, const AFK_Frame& currentFrame);
    virtual void evicted(const int row);
};

boost::shared_ptr<AFK_JigsawMetadata> afk_newLandscapeJigsawMeta(const Vec2<int>& _jigsawSize);

#endif /* _AFK_LANDSCAPE_JIGSAW_H_ */

