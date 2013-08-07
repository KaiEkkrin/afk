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
#include "def.hpp"
#include "display.hpp"
#include "jigsaw.hpp"
#include "landscape_sizes.hpp"
#include "terrain.hpp"
#include "tile.hpp"
#include "world.hpp"

#define TERRAIN_TILES_PER_TILE 5

#ifndef AFK_LANDSCAPE_CACHE
#define AFK_LANDSCAPE_CACHE AFK_EvictableCache<AFK_Tile, AFK_LandscapeTile, AFK_HashTile>
#endif

class AFK_DisplayedLandscapeTile;

/* This occurs if we can't find a tile while trying to chain
 * together the terrain.
 */
class AFK_LandscapeTileNotPresentException: public std::exception {};

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

    /* These are the lower and upper y-bounds of the vertices in
     * world space.
     * TODO I'm about to kill of my way of updating these with the
     * computed results, and I need to put that back -- it's useful
     * to have this result here to keep the display queue
     * un-cluttered.
     */
    float yBoundLower;
    float yBoundUpper;
    
public:
    AFK_LandscapeTile();
    virtual ~AFK_LandscapeTile();

    bool hasTerrainDescriptor() const;

    /* Adds a terrain descriptor to this tile if there isn't any already. */
    void makeTerrainDescriptor(
        unsigned int pointSubdivisionFactor,
        unsigned int subdivisionFactor,
        const AFK_Tile& tile,
        float minCellSize,
        const Vec3<float> *forcedTint);

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
    AFK_JigsawPiece getJigsawPiece(AFK_JigsawCollection *_jigsaws);

    bool hasArtwork() const;
    float getYBoundLower() const;
    float getYBoundUpper() const;

    /* Produces a displayed landscape tile that will render the portion
     * of this landscape tile that fits into the given cell.
     * You own the returned pointer and should delete it after
     * rendering.
     * Returns NULL if there is no landscape to be displayed
     * in that cell.
     */
    AFK_DisplayedLandscapeTile *makeDisplayedLandscapeTile(const AFK_Cell& cell, float minCellSize);

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);

#endif /* _AFK_LANDSCAPE_TILE_H_ */

