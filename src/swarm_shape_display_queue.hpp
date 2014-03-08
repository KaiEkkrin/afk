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

#ifndef _AFK_SWARM_SHAPE_DISPLAY_QUEUE_H_
#define _AFK_SWARM_SHAPE_DISPLAY_QUEUE_H_

#include "afk.hpp"

#include <mutex>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_swarm_shape_base.hpp"
#include "def.hpp"
#include "jigsaw.hpp"
#include "shader.hpp"

#ifdef _WIN32
_declspec(align(16))
#endif
class AFK_SwarmShapeDisplayUnit
{
protected:
    /* This transform describes where this particular blob is in
    * the world.
    */
    Mat4<float>         transform;

    /* Displacement relative to the shape's base cube. */
    Vec4<float>         location;

    /* This maps it onto the vapour jigsaw. */
    Vec3<float>         vapourJigsawPieceSTR;

public:
    AFK_SwarmShapeDisplayUnit(
        const Mat4<float>& _transform,
        const Vec4<float>& _location,
        const Vec3<float>& _vapourJigsawPieceSTR);
}
#ifdef __GNUC__
__attribute__((aligned(16)))
#endif
;

#define AFK_SWARM_SHAPE_DISPLAY_UNIT_SIZE (24 * sizeof(float))
static_assert(AFK_SWARM_SHAPE_DISPLAY_UNIT_SIZE == sizeof(AFK_SwarmShapeDisplayUnit), "SSDU size");

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_SwarmShapeDisplayUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_SwarmShapeDisplayUnit>::value));

class AFK_SwarmShapeDisplayQueue
{
protected:
    std::vector<AFK_SwarmShapeDisplayUnit> queue;

    /* Buffer to transport my large instanced draw operation. */
    GLuint buf;
    std::mutex mut;

    GLuint jigsawFeatureSamplerLocation;
    GLuint tboSamplerLocation;
    GLuint jigsawPiecePitchLocation;
    GLuint edgeThresholdLocation;

public:
    AFK_SwarmShapeDisplayQueue();
    virtual ~AFK_SwarmShapeDisplayQueue();

    void add(const AFK_SwarmShapeDisplayUnit& _unit);

    /* Assuming the VAO for the shape faces has been set up, this
    * function draws the parts of the entities represented by this
    * queue.
    */
    void draw(
        AFK_ShaderProgram *shaderProgram,
        AFK_Jigsaw *vapourJigsaw,
        const AFK_3DSwarmShapeBase *baseShape,
        const AFK_ShapeSizes& sSizes);

    bool empty(void);
    void clear(void);
};

#endif /* _AFK_SWARM_SHAPE_DISPLAY_QUEUE_H_ */