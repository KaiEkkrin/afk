/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TERRAIN_BASE_TILE_H_
#define _AFK_TERRAIN_BASE_TILE_H_

#include "afk.hpp"

#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "landscape_sizes.hpp"

/* The terrain basis contains (vertex location, tile coord). */

/* TODO Move the VAO currently in world so that it's owned by
 * this module instead?
 * (Surely makes more sense)
 */
class AFK_TerrainBaseTileVertex
{
public:
    Vec3<float> location;
    Vec2<float> tileCoord;

    AFK_TerrainBaseTileVertex(
        const Vec3<float>& _location,
        const Vec2<float>& _tileCoord);

} __attribute__((aligned(16)));

/* I don't trust the `sizeof' builtin to work correctly with
 * the above `aligned' ...
 */
#define AFK_TER_BASE_VERTEX_SIZE 32

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_TerrainBaseTileVertex>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_TerrainBaseTileVertex>::value));

/* This little object describes the base geometry of a terrain tile. */

class AFK_TerrainBaseTile
{
protected:
    std::vector<AFK_TerrainBaseTileVertex> vertices;
    std::vector<unsigned short> indices;

    GLuint *bufs;
public:
    AFK_TerrainBaseTile(const AFK_LandscapeSizes& lSizes);
    virtual ~AFK_TerrainBaseTile();

    /* Assuming you've got the correct VAO selected (or are
     * about to make a draw call), sets up these vertices and
     * indices with the GL with the correct properties.
     */
    void initGL(void);

    /* Assuming all the textures are set up, issues the draw call. */
    void draw(unsigned int instanceCount) const;

    /* Tears down the VAO. */
    void teardownGL(void) const;
};

#endif /* _AFK_TERRAIN_BASE_TILE_H_ */

