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

#ifndef _AFK_3DEDGESHAPE_BASE_H_
#define _AFK_3DEDGESHAPE_BASE_H_

#include "afk.hpp"

#include <vector>

#include "def.hpp"
#include "shape_sizes.hpp"

/* This class defines the basis of a 3D edge-detected cube, used to
 * make random Shapes.
 * To match the output of shape_3dedge, this base shape will
 * paint six faces, whose co-ordinates are arranged in the edge jigsaw
 * in a 3 (x) by 2 (y) formation:
 * back     right   top
 * bottom   left    front
 * The vertices are texture co-ordinates only, with the full 3D world
 * co-ordinates read from the texture.  I've done this even though
 * many of them are predictable to avoid really torturous logic in
 * the shaders.
 *
 * In order to avoid overlap of triangles from different faces, the
 * overlap edge texture will contain the face identifier each group of
 * vertices is homed to, so that the geometry shader can drop it from
 * other faces.  Because I want to group vertices in 4s, I'm going to
 * counter-intuitively use a line_adjacency primitive, which the geometry
 * shader can transform into a triangle_strip for proper rendering.
 */

class AFK_3DEdgeShapeBase
{
protected:
    std::vector<Vec2<float> > vertices;
    std::vector<unsigned short> indices;

    GLuint vertexArray;
    GLuint *bufs;

public:
    AFK_3DEdgeShapeBase(const AFK_ShapeSizes& sSizes);
    virtual ~AFK_3DEdgeShapeBase();

    /* Assuming you've got the right VAO selected, sets up the vertices
     * and indices.
     */
    void initGL(void);

    /* Assuming all the textures are set up, issues the draw call. */
    void draw(size_t instanceCount) const;

    /* Tears down the VAO. */
    void teardownGL(void) const;
};

#endif /* _AFK_3DEDGESHAPE_BASE_H_ */

