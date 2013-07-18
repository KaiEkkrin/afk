/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_TILE_H_
#define _AFK_LANDSCAPE_TILE_H_

#include "afk.hpp"

#include <exception>
#include <iostream>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "data/claimable.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "display.hpp"
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

#define AFK_LANDSCAPE_VERTEX_BUF AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>
#define AFK_LANDSCAPE_INDEX_BUF AFK_DisplayedBuffer<Vec3<unsigned int> >

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
    bool hasTerrainDescriptor;
    std::vector<boost::shared_ptr<AFK_TerrainTile> > terrainDescriptor;

    /* This is where I store the raw geometry, while the terrain
     * is being computed on it.
     */
    Vec3<float> *rawVertices;
    Vec3<float> *rawColours;
    unsigned int rawVertexCount;

    /* Here's the computed vertex data */
    boost::shared_ptr<AFK_LANDSCAPE_VERTEX_BUF> vs;

    /* These are the lower and upper y-bounds of the vertices in
     * world space.
     */
    float yBoundLower;
    float yBoundUpper;

    /* Here's the computed index buffer. */
    boost::shared_ptr<AFK_LANDSCAPE_INDEX_BUF> is;

    /* Internal helper.
     * Computes a single flat triangle, pushing the results into
     * the vertex and index buffers.
     */
    void computeFlatTriangle(
        const Vec3<float> *vertices,
        const Vec3<float> *colours,
        const Vec3<unsigned int>& indices,
        unsigned int triangleVOff);

    /* Internal helper.
     * Turns a vertex grid into a world of flat triangles,
     * filling out `vs' and `is', and updating yBoundLower
     * and yBoundUpper.
     */
    void vertices2FlatTriangles(
        const AFK_Tile& baseTile,
        unsigned int pointSubdivisionFactor);

    /* Internal helper.  Makes this landscape tile's
     * raw terrain data (vertices and colours).
     */
    void makeRawTerrain(const AFK_Tile& baseTile, unsigned int pointSubdivisionFactor);
    
public:
    AFK_LandscapeTile();
    virtual ~AFK_LandscapeTile();

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
        AFK_TerrainList& list,
        const AFK_Tile& tile,
        unsigned int subdivisionFactor,
        float maxDistance,
        const AFK_LANDSCAPE_CACHE *cache) const;

    /* Returns true if there is raw terrain here that needs computing,
     * else false.  As a side effect, computes the initial raw
     * terrain grid if required.
     */
    bool hasRawTerrain(
        const AFK_Tile& baseTile,
        unsigned int pointSubdivisionFactor);

    /* Makes this cell's terrain geometry.  Call right after
     * constructing.
     */
    void computeGeometry(
        unsigned int pointSubdivisionFactor,
        const AFK_Tile& baseTile,
        const AFK_TerrainList& terrainList);

    bool hasGeometry() const;

    /* Produces a displayed landscape tile that will render the portion
     * of this landscape tile that fits into the given cell.
     * You own the returned pointer and should delete it after
     * rendering.
     * Returns NULL if there is no landscape to be displayed
     * in that cell.
     */
    AFK_DisplayedLandscapeTile *makeDisplayedLandscapeTile(const AFK_Cell& cell, float minCellSize) const;

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeTile& t);

#endif /* _AFK_LANDSCAPE_TILE_H_ */
