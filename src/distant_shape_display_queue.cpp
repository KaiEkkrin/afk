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

#include "debug.hpp"
#include "display.hpp"
#include "distant_shape_display_queue.hpp"

AFK_DistantShapeDisplayUnit::AFK_DistantShapeDisplayUnit(
    const Mat4<float>& _transform,
    const Vec4<float>& _location,
    const Vec3<float>& _colour) :
    transform(_transform),
    location(_location),
    colour(_colour)
{
}

AFK_DistantShapeDisplayQueue::AFK_DistantShapeDisplayQueue() :
buf(0),
tboSamplerLocation(0)
{
}

AFK_DistantShapeDisplayQueue::~AFK_DistantShapeDisplayQueue()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_DistantShapeDisplayQueue::add(const AFK_DistantShapeDisplayUnit& _unit)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.push_back(_unit);
}

void AFK_DistantShapeDisplayQueue::draw(
    AFK_ShaderProgram *shaderProgram,
    const AFK_3DDistantShapeBase *baseShape,
    const AFK_ShapeSizes& sSizes)
{
    size_t instanceCount = queue.size();
    if (instanceCount == 0) return;

    if (!tboSamplerLocation)
    {
        tboSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
    }

    /* Set up the distant shape texbuf. */
    glActiveTexture(GL_TEXTURE0);
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
        queue.size() * AFK_DISTANT_SHAPE_DISPLAY_UNIT_SIZE,
        queue.data(),
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("distant shape display queue texBuffer");
    glUniform1i(tboSamplerLocation, 0);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif

    baseShape->draw(instanceCount);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

bool AFK_DistantShapeDisplayQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return queue.empty();
}

void AFK_DistantShapeDisplayQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.clear();
}
