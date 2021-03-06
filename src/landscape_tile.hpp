/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#ifndef _AFK_LANDSCAPE_TILE_H_
#define _AFK_LANDSCAPE_TILE_H_

#include "afk.hpp"

#include <array>
#include <exception>
#include <iostream>

#include "data/claimable.hpp"
#include "data/evictable_cache.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "display.hpp"
#include "jigsaw_collection.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_sizes.hpp"
#include "terrain.hpp"
#include "tile.hpp"
#include "world.hpp"

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

/* Utilities. */
ptrdiff_t afk_getLandscapeTileFeaturesOffset(void);
ptrdiff_t afk_getLandscapeTileTilesOffset(void);

/* Describes a landscape tile, including managing its rendered vertex
 * and index buffers.
 */
class AFK_LandscapeTile
{
protected:
    /* The structure that describes the terrain present at this
     * tile.
     * (The actual terrain to be rendered will be a combination of
     * the terrain here and the terrain at all the ancestral tiles.)
     */
    bool haveTerrainDescriptor;

    typedef std::array<AFK_TerrainFeature, afk_terrainFeatureCountPerTile * afk_terrainTilesPerTile> FeatureArray;
    FeatureArray terrainFeatures;
    typedef std::array<AFK_TerrainTile, afk_terrainTilesPerTile> TileArray;
    TileArray terrainTiles;

    /* The jigsaw piece for this tile, or the null piece if it hasn't
     * been assigned yet.
     */
    AFK_JigsawPiece jigsawPiece;

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
     * If tiles are missing, fills out the `missing' list:
     * you'll need to render all those tiles then resume.
     */
    void buildTerrainList(
        unsigned int threadId,
        AFK_TerrainList& list,
        const AFK_Tile& tile,
        unsigned int subdivisionFactor,
        float maxDistance,
        AFK_LANDSCAPE_CACHE *cache,
        std::vector<AFK_Tile>& missing) const;

    /* This version so that I can use an inplace claim and
     * save a copy.
     */
    void buildTerrainList(
        unsigned int threadId,
        AFK_TerrainList& list,
        const AFK_Tile& tile,
        unsigned int subdivisionFactor,
        float maxDistance,
        AFK_LANDSCAPE_CACHE *cache,
        std::vector<AFK_Tile>& missing) const volatile;

    /* Assigns a jigsaw piece to this tile. */
    AFK_JigsawPiece getJigsawPiece(unsigned int threadId, int minJigsaw, AFK_JigsawCollection *_jigsaws);

    enum AFK_LandscapeTileArtworkState artworkState(AFK_JigsawCollection *jigsaws) const;
    float getYBoundLower() const { return yBoundLower; }
    float getYBoundUpper() const { return yBoundUpper; }

    /* Supply new y bounds _in tile space_ (that's easiest
     * for the yReduce kernel)
     */
    void setYBounds(float _yBoundLower, float _yBoundUpper);

    /* Returns true if any of the given real cell co-ordinates
     * are within y bounds, else false.
     */
    bool realCellWithinYBounds(const Vec4<float>& coord) const;
    bool realCellWithinYBounds(const Vec4<float>& coord) const volatile;

    /* Checks whether this landscape tile has anything to render in
     * the given cell (by y-bounds).  If not, returns false.  If so,
     * fills out `o_unit' with an enqueueable display unit, `o_jigsawPiece'
     * with the relevant jigsaw piece, and returns true.
     */
    bool makeDisplayUnit(
        const AFK_Cell& cell,
        float minCellSize,
        AFK_JigsawCollection *jigsaws,
        AFK_JigsawPiece& o_jigsawPiece,
        AFK_LandscapeDisplayUnit& o_unit) const;

    /* For handling claiming and eviction. */
    void evict(void);

    friend ptrdiff_t afk_getLandscapeTileFeaturesOffset(void);
    friend ptrdiff_t afk_getLandscapeTileTilesOffset(void);
    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);

#endif /* _AFK_LANDSCAPE_TILE_H_ */

