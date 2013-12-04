/* AFK
 * Copyright (C) 2013, Alex Holloway.
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
#ifdef _WIN32
_declspec(align(16))
#endif
class AFK_TerrainBaseTileVertex
{
public:
    Vec3<float> location;
    Vec2<float> tileCoord;

    AFK_TerrainBaseTileVertex(
        const Vec3<float>& _location,
        const Vec2<float>& _tileCoord);

}
#ifdef __GNUC__
__attribute__((aligned(16)))
#endif
;

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
    void draw(size_t instanceCount) const;

    /* Tears down the VAO. */
    void teardownGL(void) const;
};

#endif /* _AFK_TERRAIN_BASE_TILE_H_ */

