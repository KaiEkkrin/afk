/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_TILE_H_
#define _AFK_LANDSCAPE_TILE_H_

#include "afk.hpp"

#include <exception>
#include <iostream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/shared_ptr.hpp>

#include "data/claimable.hpp"
#include "data/frame.hpp"
#include "data/polymer_cache.hpp"
#include "def.hpp"
#include "display.hpp"
#include "jigsaw_collection.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_sizes.hpp"
#include "terrain.hpp"
#include "tile.hpp"
#include "world.hpp"

#define TERRAIN_TILES_PER_TILE 5

#ifndef AFK_LANDSCAPE_CACHE
#define AFK_LANDSCAPE_CACHE AFK_EvictableCache<AFK_Tile, AFK_LandscapeTile, AFK_HashTile>
#endif

class AFK_LandscapeDisplayUnit;

/* This occurs if we can't find a tile while trying to chain
 * together the terrain.
 */
class AFK_LandscapeTileNotPresentException: public std::exception {};

/* A tile's possible artwork state. */
enum AFK_LandscapeTileArtworkState
{
    AFK_LANDSCAPE_TILE_NO_PIECE_ASSIGNED,
    AFK_LANDSCAPE_TILE_PIECE_SWEPT,
    AFK_LANDSCAPE_TILE_HAS_ARTWORK
};

/* Describes a landscape tile, including managing its rendered vertex
 * and index buffers.
 */
class AFK_LandscapeTile: public AFK_Claimable
{
protected:
    /* The structure that describes the terrain present at this
     * tile.
     * (The actual terrain to be rendered will be a combination of
     * the terrain here and the terrain at all the ancestral tiles.)
     */
    bool haveTerrainDescriptor;
    std::vector<AFK_TerrainFeature> terrainFeatures; /* landscapeSizes->featureCountPerTile features per tile, in order */
    std::vector<AFK_TerrainTile> terrainTiles;

    /* The jigsaw piece for this tile, or the null piece if it hasn't
     * been assigned yet.
     */
    AFK_JigsawPiece jigsawPiece;

    /* What jigsaws that piece comes from.  (This is just a cross-
     * reference, we don't own it.  But we need to be able to put
     * our piece back upon delete.)
     */
    AFK_JigsawCollection *jigsaws;

    /* The frame when we got that jigsaw piece.  Jigsaw pieces expire
     * as the new-piece creation row rolls around; when we query the
     * jigsaw for the timestamp of our piece and it turns out to be
     * newer than this, we need to re-generate the piece.
     */
    AFK_Frame jigsawPieceTimestamp;

    /* These are the lower and upper y-bounds of the vertices in
     * world space.
     */
    float yBoundLower;
    float yBoundUpper;
    
public:
    AFK_LandscapeTile();
    virtual ~AFK_LandscapeTile();

    bool hasTerrainDescriptor() const;

    /* Adds a terrain descriptor to this tile if there isn't any already. */
    void makeTerrainDescriptor(
        const AFK_LandscapeSizes& lSizes,
        const AFK_Tile& tile,
        float minCellSize);

    /* Builds the terrain list for this tile.  Call it with
     * an empty list.
     */
    void buildTerrainList(
        unsigned int threadId,
        AFK_TerrainList& list,
        const AFK_Tile& tile,
        unsigned int subdivisionFactor,
        float maxDistance,
        const AFK_LANDSCAPE_CACHE *cache) const;

    /* Assigns a jigsaw piece to this tile. */
    AFK_JigsawPiece getJigsawPiece(unsigned int threadId, int minJigsaw, AFK_JigsawCollection *_jigsaws);

    enum AFK_LandscapeTileArtworkState artworkState() const;
    float getYBoundLower() const;
    float getYBoundUpper() const;

    /* Supply new y bounds _in tile space_ (that's easiest
     * for the yReduce kernel)
     */
    void setYBounds(float _yBoundLower, float _yBoundUpper);

    /* Returns true if any of the given real cell co-ordinates
     * are within y bounds, else false.
     */
    bool realCellWithinYBounds(const Vec4<float>& coord) const;

    /* Checks whether this landscape tile has anything to render in
     * the given cell (by y-bounds).  If not, returns false.  If so,
     * fills out `o_unit' with an enqueueable display unit, `o_jigsawPiece'
     * with the relevant jigsaw piece, and returns true.
     */
    bool makeDisplayUnit(
        const AFK_Cell& cell,
        float minCellSize,
        AFK_JigsawPiece& o_jigsawPiece,
        AFK_LandscapeDisplayUnit& o_unit) const;

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);

#endif /* _AFK_LANDSCAPE_TILE_H_ */

