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

#include <cassert>

#include "3d_swarm_shape_base.hpp"
#include "display.hpp"

static unsigned short getIndexForPoint(unsigned int x, unsigned int y, unsigned int z, const AFK_ShapeSizes& sSizes)
{
    unsigned int idx = (x * SQUARE(sSizes.eDim) + y * sSizes.eDim + z);
    assert(idx <= USHRT_MAX);
    return static_cast<unsigned short>(idx);
}

AFK_3DSwarmShapeBase::AFK_3DSwarmShapeBase(const AFK_ShapeSizes& sSizes):
bufs(nullptr)
{
    for (unsigned int x = 0; x < sSizes.eDim; ++x)
    {
        for (unsigned int y = 0; y < sSizes.eDim; ++y)
        {
            for (unsigned int z = 0; z < sSizes.eDim; ++z)
            {
                vertices.push_back(afk_vec3<float>(
                    (float)x / (float)sSizes.pointSubdivisionFactor,
                    (float)y / (float)sSizes.pointSubdivisionFactor,
                    (float)z / (float)sSizes.pointSubdivisionFactor));
            }
        }
    }

    for (unsigned int x = 0; x < sSizes.pointSubdivisionFactor; ++x)
    {
        for (unsigned int y = 0; y < sSizes.pointSubdivisionFactor; ++y)
        {
            for (unsigned int z = 0; z < sSizes.pointSubdivisionFactor; ++z)
            {
                indices.push_back(getIndexForPoint(x, y, z, sSizes));
                indices.push_back(getIndexForPoint(x + 1, y, z, sSizes));
                indices.push_back(getIndexForPoint(x, y + 1, z, sSizes));
                indices.push_back(getIndexForPoint(x, y, z + 1, sSizes));
            }
        }
    }
}

AFK_3DSwarmShapeBase::~AFK_3DSwarmShapeBase()
{
    if (bufs)
    {
        glDeleteBuffers(2, bufs);
    }
}

void AFK_3DSwarmShapeBase::initGL(void)
{
    bool needBufferPush = (bufs == nullptr);
    if (needBufferPush)
    {
        bufs = new GLuint[2];
        glGenBuffers(2, bufs);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vec3<float>), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    if (needBufferPush)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vec3<float>), 0);
}

void AFK_3DSwarmShapeBase::draw(size_t instanceCount) const
{
    glDrawElementsInstanced(GL_LINES_ADJACENCY, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_SHORT, 0, static_cast<GLsizei>(instanceCount));
    AFK_GLCHK("3D swarm shape draw");
}

void AFK_3DSwarmShapeBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
}
