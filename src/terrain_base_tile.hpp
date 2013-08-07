/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "landscape_sizes.hpp"

/* The terrain basis contains (vertex location, tile coord). */

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
#define SIZEOF_BASE_VERTEX 32

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

    void teardownGL(void) const;
};

