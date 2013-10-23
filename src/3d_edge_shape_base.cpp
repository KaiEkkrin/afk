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

#include "afk.hpp"

#include "3d_edge_shape_base.hpp"
#include "display.hpp"

AFK_3DEdgeShapeBaseVertex::AFK_3DEdgeShapeBaseVertex(
    const Vec2<float>& _texCoord,
    const Vec2<int>& _meta):
        texCoord(_texCoord), meta(_meta)
{
}

AFK_3DEdgeShapeBase::AFK_3DEdgeShapeBase(const AFK_ShapeSizes& sSizes):
    bufs(nullptr)
{
    /* I make six faces, each distinguished only by the face
     * identifier (x field of the meta.)
     */
    for (int face = 0; face < 6; ++face)
    {
        for (unsigned int x = 0; x < sSizes.vDim; ++x)
        {
            for (unsigned int z = 0; z < sSizes.vDim; ++z)
            {
                vertices.push_back(AFK_3DEdgeShapeBaseVertex(
                    afk_vec2<float>(
                        (float)x / (float)sSizes.eDim,
                        (float)z / (float)sSizes.eDim),
                    afk_vec2<int>(face, 0)));
            }
        }
    }
}

AFK_3DEdgeShapeBase::~AFK_3DEdgeShapeBase()
{
    if (bufs)
    {
        glDeleteBuffers(2, &bufs[0]);
        delete[] bufs;
    }
}

void AFK_3DEdgeShapeBase::initGL()
{
    bool needBufferPush = (bufs == nullptr);
    if (needBufferPush)
    {
        bufs = new GLuint[2];
        glGenBuffers(2, bufs);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(AFK_3DEdgeShapeBaseVertex), &vertices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(AFK_3DEdgeShapeBaseVertex), 0);
    glVertexAttribPointer(1, 2, GL_INT, GL_FALSE, sizeof(AFK_3DEdgeShapeBaseVertex), (GLvoid *)sizeof(Vec2<float>));
}

void AFK_3DEdgeShapeBase::draw(unsigned int instanceCount) const
{
    glDrawArraysInstanced(GL_POINTS, 0, vertices.size(), instanceCount);
    AFK_GLCHK("3d edge shape draw")
}

void AFK_3DEdgeShapeBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

