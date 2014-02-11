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

#include "3d_distant_shape_base.hpp"
#include "display.hpp"

AFK_3DDistantShapeBase::AFK_3DDistantShapeBase(const AFK_ShapeSizes& sSizes) :
buf(0)
{
    vertex = afk_vec3<float>(0.0f, 0.0f, 0.0f);

}

AFK_3DDistantShapeBase::~AFK_3DDistantShapeBase()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_3DDistantShapeBase::initGL(void)
{
    bool needBufferPush = (buf == 0);
    if (needBufferPush)
    {
        glGenBuffers(1, &buf);
    }

    glBindBuffer(GL_ARRAY_BUFFER, buf);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex), &vertex, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), 0);
}

void AFK_3DDistantShapeBase::draw(size_t instanceCount) const
{
    glDrawArraysInstanced(GL_POINTS, 0, 1, static_cast<GLsizei>(instanceCount));
    AFK_GLCHK("3D distant shape draw");
}

void AFK_3DDistantShapeBase::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
}
