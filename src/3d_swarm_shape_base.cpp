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

#include "afk.hpp"

#include "3d_swarm_shape_base.hpp"
#include "display.hpp"

AFK_3DSwarmShapeBase::AFK_3DSwarmShapeBase(const AFK_ShapeSizes& sSizes) :
buf(0)
{
    for (unsigned int x = 0; x < sSizes.pointSubdivisionFactor; ++x)
    {
        for (unsigned int y = 0; y < sSizes.pointSubdivisionFactor; ++y)
        {
            for (unsigned int z = 0; z < sSizes.pointSubdivisionFactor; ++z)
            {
                vertices.push_back(afk_vec3<float>(
                    (float)x / (float)sSizes.eDim,
                    (float)y / (float)sSizes.eDim,
                    (float)z / (float)sSizes.eDim));
            }
        }
    }
}

AFK_3DSwarmShapeBase::~AFK_3DSwarmShapeBase()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_3DSwarmShapeBase::initGL(void)
{
    bool needBufferPush = (buf == 0);
    if (needBufferPush)
    {
        glGenBuffers(1, &buf);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buf);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vec3<float>), vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3<float>), 0);
}

void AFK_3DSwarmShapeBase::draw(size_t instanceCount) const
{
    glDrawArraysInstanced(GL_POINTS, 0, static_cast<GLsizei>(vertices.size()), static_cast<GLsizei>(instanceCount));
    AFK_GLCHK("3D swarm shape draw");
}

void AFK_3DSwarmShapeBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
}
