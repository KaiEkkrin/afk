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

/* This contains a Landscape Tile's geometry buffers. */
class AFK_LandscapeGeometry
{
public:
    AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>     vs;
    AFK_DisplayedBuffer<Vec3<unsigned int> >            is;

    AFK_LandscapeGeometry(size_t vCount, size_t iCount);
};

enum AFK_LandscapeType
{
    AFK_LANDSCAPE_TYPE_FLAT,
    AFK_LANDSCAPE_TYPE_SMOOTH
};

/* TODO This really ought to be configurable somehow but I can't find
 * a sane way to do it.  (access config directly?  it IS kind of the
 * right paradigm?)
 */
#define AFK_LANDSCAPE_TYPE AFK_LANDSCAPE_TYPE_SMOOTH

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
    std::vector<boost::shared_ptr<AFK_TerrainTile> > terrainDescriptor;

    /* This is where I store the raw geometry, while the terrain
     * is being computed on it.
     */
    Vec3<float> *rawVertices;
    Vec3<float> *rawColours;
    unsigned int rawVertexCount;

    /* Here's the computed geometry data */
    AFK_LandscapeGeometry *geometry;

    /* These are the lower and upper y-bounds of the vertices in
     * world space.
     */
    float yBoundLower;
    float yBoundUpper;

    /* Here's the overall promise of a computed landscape tile that
     * can be displayed.  With this, I can enqueue a future landscape
     * tile into the render queue before I've finished computing it.
     * This was going to be a future, but I can't make shared_future
     * work, so right now it's just a horrible expectation
     * that the pointer will stop being NULL at some point before
     * render :P
     */
    boost::atomic<AFK_LandscapeGeometry**> futureGeometry;

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
     * Computes a single smooth triangle, pushing the results into
     * the vertex and index buffers.
     */
    void computeSmoothTriangle(
        const Vec3<unsigned int>& indices,
        bool emitIndices);

    /* Internal helper.
     * Turns a vertex grid into a world of flat triangles,
     * filling out `vs' and `is', and updating yBoundLower
     * and yBoundUpper.
     */
    void vertices2FlatTriangles(
        const AFK_Tile& baseTile,
        unsigned int pointSubdivisionFactor);

    /* Internal helper.
     * Turns a vertex grid into a world of smooth triangles,
     * filling out `vs' and `is', and updating yBoundLower
     * and yBoundUpper.
     */
    void vertices2SmoothTriangles(
        const AFK_Tile& baseTile,
        unsigned int pointSubdivisionFactor);
    
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

    /* Returns true if there is geometry here that needs making and
     * you are the thread chosen to make it.  Otherwise, returns
     * false.
     * (Call hasGeometry() to find out if there will be geometry here
     * by the time it's needed; another thread might be making it.)
     */
    bool claimGeometryRights(void);

    /* --- Begin functions you should call if you got geometry rights --- */

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

    /* Makes this landscape tile's
     * raw terrain data (vertices and colours).
     * It has a `type' parameter because the smooth
     * terrain needs an extra row of vertices in
     * order to make the normals for the bottom and
     * left.
     */
    void makeRawTerrain(
        enum AFK_LandscapeType type,
        const AFK_Tile& baseTile,
        unsigned int pointSubdivisionFactor,
        float minCellSize);

    /* Makes this cell's terrain geometry.  Call right after
     * constructing.
     */
    void computeGeometry(
        unsigned int pointSubdivisionFactor,
        const AFK_Tile& baseTile,
        const AFK_TerrainList& terrainList);

    /* --- End functions you should call if you got geometry rights --- */

    bool hasGeometry() const;

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

