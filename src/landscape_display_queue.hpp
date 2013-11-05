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

#ifndef _AFK_LANDSCAPE_DISPLAY_QUEUE_H_
#define _AFK_LANDSCAPE_DISPLAY_QUEUE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "landscape_sizes.hpp"
#include "shader.hpp"
#include "terrain_base_tile.hpp"
#include "tile.hpp"

class AFK_Jigsaw;
class AFK_LandscapeTile;

/* This module is like terrain_compute_queue, but for queueing up
 * the cell-specific landscape details -- cell coord, jigsaw STs,
 * y bounds -- in a texture buffer that can be shoveled into the
 * GL in one go.
 * The world will have one of these queues per jigsaw organised in
 * a fair, just like terrain_compute_queue.
 */

class AFK_LandscapeDisplayUnit
{
public:
    Vec4<float>     cellCoord;
    Vec2<float>     jigsawPieceST; /* between 0 and 1 in jigsaw space */
    float           yBoundLower;
    float           yBoundUpper;

    AFK_LandscapeDisplayUnit();
    AFK_LandscapeDisplayUnit(
        const Vec4<float>& _cellCoord,
        const Vec2<float>& _jigsawPieceST,
        float _yBoundLower,
        float _yBoundUpper);

    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_LandscapeDisplayUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_LandscapeDisplayUnit>::value));

class AFK_LandscapeDisplayQueue
{
protected:
    std::vector<AFK_LandscapeDisplayUnit> queue;
    std::vector<AFK_Tile> landscapeTiles; /* for last-moment y bounds fetch */
    GLuint buf;
    boost::mutex mut;

    /* After culling cells that are entirely outside y-bounds, the shortened
     * queue goes here
     */
    std::vector<AFK_LandscapeDisplayUnit> culledQueue;

    /* Drawing stuff. */
    GLuint jigsawPiecePitchLocation;
    GLuint jigsawYDispTexSamplerLocation;
    GLuint jigsawColourTexSamplerLocation;
    GLuint jigsawNormalTexSamplerLocation;
    GLuint displayTBOSamplerLocation;

public:
    AFK_LandscapeDisplayQueue();
    virtual ~AFK_LandscapeDisplayQueue();

    /* The cell enumerator workers should call this to add a unit
     * for rendering.
     */
    void add(const AFK_LandscapeDisplayUnit& _unit, const AFK_Tile& _tile);

    /* This function draws the part of the landscape represented by
     * this queue, assuming that the basic VAO for the landscape tiles
     * has already been selected
     */
    void draw(AFK_ShaderProgram *shaderProgram, AFK_Jigsaw* jigsaw, const AFK_TerrainBaseTile *baseTile, const AFK_LandscapeSizes& lSizes);

    bool empty(void);

    void clear(void);
};

#endif /* _AFK_LANDSCAPE_DISPLAY_QUEUE_H_ */

