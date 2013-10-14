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

#ifndef _AFK_ENTITY_DISPLAY_QUEUE_H_
#define _AFK_ENTITY_DISPLAY_QUEUE_H_

#include "afk.hpp"

#include <vector>

#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_edge_shape_base.hpp"
#include "def.hpp"
#include "shader.hpp"
#include "shape_sizes.hpp"

class AFK_Jigsaw;

/* An EntityDisplayQueue, like a LandscapeDisplayQueue, represents a
 * list of bits of shrink-form shapes, ready to be displayed.
 * Each "bit" (`unit') is one face.
 */

class AFK_EntityDisplayUnit
{
protected:
    /* This transform describes where this particular face is in
     * the world.
     */
    Mat4<float>         transform;

    /* This maps it onto the vapour jigsaw. */
    Vec3<float>         vapourJigsawPieceSTR;

    /* This maps it onto the edge jigsaw. */
    Vec2<float>         edgeJigsawPieceST;

public:
    AFK_EntityDisplayUnit(
        const Mat4<float>& _transform,
        const Vec3<float>& _vapourJigsawPieceSTR,
        const Vec2<float>& _edgeJigsawPieceST);

    //friend std::ostream& operator<<(std::ostream& os, const AFK_EntityDisplayUnit& unit);
} __attribute__((aligned(16)));

#define ENTITY_DISPLAY_UNIT_SIZE (20 * sizeof(float))

//std::ostream& operator<<(std::ostream& os, const AFK_EntityDisplayUnit& unit);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_EntityDisplayUnit>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_EntityDisplayUnit>::value));

class AFK_EntityDisplayQueue
{
protected:
    std::vector<AFK_EntityDisplayUnit> queue;

    /* Like with the landscape display queue, the entity display queue is
     * fed to the GL as a texture buffer, in a large instanced draw call,
     * so that the GL can make lots of instances of the shrink-form shape
     * face geometry and mould each one based on the displacements in the
     * entity jigsaw.
     * (Said entity jigsaw, of course, has similar surfacing to the
     * landscape jigsaw, but rather than having merely y-displacements,
     * has displacements in all three directions!)
     */
    GLuint buf;
    boost::mutex mut;

    GLuint vapourJigsawPiecePitchLocation;
    GLuint edgeJigsawPiecePitchLocation;
    GLuint jigsawDispTexSamplerLocation;
    GLuint jigsawDensityTexSamplerLocation;
    GLuint jigsawNormalTexSamplerLocation;
    GLuint jigsawOverlapTexSamplerLocation;
    GLuint displayTBOSamplerLocation;

public:
    AFK_EntityDisplayQueue();
    virtual ~AFK_EntityDisplayQueue();

    void add(const AFK_EntityDisplayUnit& _unit);

    /* Assuming the VAO for the shape faces has been set up, this
     * function draws the parts of the entities represented by this
     * queue.
     */
    void draw(
        AFK_ShaderProgram *shaderProgram,
        AFK_Jigsaw *vapourJigsaw,
        AFK_Jigsaw *edgeJigsaw,
        const AFK_3DEdgeShapeBase *baseShape,
        const AFK_ShapeSizes& sSizes);

    bool empty(void);

    void clear(void);
};

#endif /* _AFK_ENTITY_DISPLAY_QUEUE_H_ */

