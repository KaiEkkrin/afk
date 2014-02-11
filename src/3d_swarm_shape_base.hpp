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

#ifndef _AFK_3D_SWARM_SHAPE_BASE_H_
#define _AFK_3D_SWARM_SHAPE_BASE_H_

#include "afk.hpp"

#include <vector>

#include "def.hpp"
#include "shape_sizes.hpp"

/* This class defines how we draw the swarm of shape points.
*/
class AFK_3DSwarmShapeBase
{
protected:
    /* This swarm of points index into the vapour. */
    std::vector<Vec3<float> > vertices;

    GLuint buf;

public:
    AFK_3DSwarmShapeBase(const AFK_ShapeSizes& sSizes);
    virtual ~AFK_3DSwarmShapeBase();

    /* Sets up the vertices and indices.  Assign a VAO first to do
    * this only once.
    */
    void initGL(void);

    /* Issues the draw call assuming everything is set up. */
    void draw(size_t instanceCount) const;

    /* Tears down the VAO. */
    void teardownGL(void) const;
};

#endif /* _AFK_3D_SWARM_SHAPE_BASE_H_ */
