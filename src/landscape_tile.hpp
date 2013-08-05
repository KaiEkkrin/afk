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
#include "gl_buffer.hpp"
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
    Vec3<float> *rawVertices;
    Vec3<float> *rawColours;

    AFK_DisplayedBuffer<struct AFK_VcolPhongVertex>     vs;
    AFK_DisplayedBuffer<struct AFK_VcolPhongIndex>      is;

    AFK_LandscapeGeometry(
        Vec3<float> *_rawVertices, Vec3<float> *_rawColours,
        size_t vCount, size_t iCount,
        AFK_GLBufferQueue *vSource, AFK_GLBufferQueue *iSource);
};

/* This utility returns the sizes of the various landscape
 * elements, so that I can configure the cache correctly, and correctly
 * drive the drawing functions.
 */
class AFK_LandscapeSizes
{
public:
    const unsigned int pointSubdivisionFactor;
    const unsigned int baseVDim; /* Number of vertices along an edge in the base tile */
    const unsigned int vDim; /* Number of vertices along an edge, including extras for computation */
    const unsigned int iDim; /* Number of triangles along an edge */
    const unsigned int baseVCount; /* Total number of vertex structures in the instanced base tile */
    const unsigned int vCount; /* Total number of vertex structures in the computed geometry */
    const unsigned int iCount; /* Total number of index structures */
    const unsigned int baseVSize; /* Size of base tile vertex array in bytes */
    const unsigned int vSize; /* Size of vertex array in bytes */
    const unsigned int iSize; /* Size of index array in bytes */

    AFK_LandscapeSizes(unsigned int pointSubdivisionFactor);
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
    std::vector<boost::shared_ptr<AFK_TerrainTile> > terrainDescriptor;

    /* The raw geometry data, while I apply the terrain
     * transform.
     * TODO This needs to go away when I've ported the
     * terrain transform to OpenCL.
     */
    Vec3<float> *rawVertices;
    Vec3<float> *rawColours;

    /* Here's the geometry data */
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
        const AFK_Tile& baseTile,
        const AFK_LandscapeSizes& lSizes,
        float minCellSize);

    /* Makes this cell's terrain geometry.  Call right after
     * constructing.
     */
    void computeGeometry(
        const AFK_Tile& baseTile,
        const AFK_LandscapeSizes& lSizes,
        const AFK_TerrainList& terrainList,
        AFK_GLBufferQueue *vSource,
        AFK_GLBufferQueue *iSource);

    /* --- End functions you should call if you got geometry rights --- */

    bool hasGeometry() const;
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

