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

#ifndef _AFK_3D_DISTANT_SHAPE_BASE_H_
#define _AFK_3D_DISTANT_SHAPE_BASE_H_

#include "afk.hpp"

#include "def.hpp"
#include "shape_sizes.hpp"

/* This class defines what we draw for distant shapes (too far away to
 * clearly see even the coarest LoD).
 * Handle it like terrain_base_tile.
 */
class AFK_3DDistantShapeBase
{
protected:
    /* The shape has just one single vertex. */
    Vec3<float> vertex;

    GLuint vertexArray;
    GLuint buf;

public:
    AFK_3DDistantShapeBase(const AFK_ShapeSizes& sSizes);
    virtual ~AFK_3DDistantShapeBase();

    /* Sets up the vertices and indices.  Assign a VAO first to do
     * this only once.
     */
    void initGL(void);

    /* Issues the draw call assuming everything is set up. */
    void draw(size_t instanceCount) const;

    /* Tears down the VAO. */
    void teardownGL(void) const;
};

#endif /* _AFK_3D_DISTANT_SHAPE_BASE_H_ */
