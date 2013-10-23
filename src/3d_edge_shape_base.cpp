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

#define RESTART_INDEX 65535

AFK_3DEdgeShapeBase::AFK_3DEdgeShapeBase(const AFK_ShapeSizes& sSizes):
    bufs(nullptr)
{
    /* I make six faces, each distinguished only by the face
     * identifier (x field of the meta.)
     */
    for (int face = 0; face < 6; ++face)
    {
        for (unsigned int x = 0; x < sSizes.eDim; ++x)
        {
            for (unsigned int z = 0; z < sSizes.eDim; ++z)
            {
                vertices.push_back(AFK_3DEdgeShapeBaseVertex(
                    afk_vec2<float>(
                        (float)x / (float)sSizes.eDim,
                        (float)z / (float)sSizes.eDim),
                    afk_vec2<int>(face, 0)));
            }
        }
    }

    for (unsigned short face = 0; face < 6; ++face)
    {
        unsigned short faceOffset = face * sSizes.eDim * sSizes.eDim;

        for (unsigned short s = 0; s < sSizes.pointSubdivisionFactor; ++s)
        {
            for (unsigned short t = 0; t < sSizes.pointSubdivisionFactor; ++t)
            {
                unsigned short i_r1c1 = faceOffset + s * sSizes.eDim + t;
                unsigned short i_r2c1 = faceOffset + (s + 1) * sSizes.eDim + t;
                unsigned short i_r1c2 = faceOffset + s * sSizes.eDim + (t + 1);
                unsigned short i_r2c2 = faceOffset + (s + 1) * sSizes.eDim + (t + 1);
        
                indices.push_back(i_r1c1);
                indices.push_back(i_r1c2);
                indices.push_back(i_r2c1);
                indices.push_back(i_r2c2);
                indices.push_back(RESTART_INDEX);
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

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    if (needBufferPush)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(AFK_3DEdgeShapeBaseVertex), 0);
    glVertexAttribPointer(1, 2, GL_INT, GL_FALSE, sizeof(AFK_3DEdgeShapeBaseVertex), (GLvoid *)sizeof(Vec2<float>));

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(RESTART_INDEX);
}

void AFK_3DEdgeShapeBase::draw(unsigned int instanceCount) const
{
    glDrawElementsInstanced(GL_LINE_STRIP_ADJACENCY, indices.size(), GL_UNSIGNED_SHORT, 0, instanceCount);
    AFK_GLCHK("3d edge shape draw")
}

void AFK_3DEdgeShapeBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

