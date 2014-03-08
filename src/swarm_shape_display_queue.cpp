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
#include "swarm_shape_display_queue.hpp"

AFK_SwarmShapeDisplayUnit::AFK_SwarmShapeDisplayUnit(
    const Mat4<float>& _transform,
    const Vec4<float>& _location,
    const Vec3<float>& _vapourJigsawPieceSTR) :
    transform(_transform),
    location(_location),
    vapourJigsawPieceSTR(_vapourJigsawPieceSTR)
{
}

AFK_SwarmShapeDisplayQueue::AFK_SwarmShapeDisplayQueue() :
buf(0),
tboSamplerLocation(0)
{
}

AFK_SwarmShapeDisplayQueue::~AFK_SwarmShapeDisplayQueue()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_SwarmShapeDisplayQueue::add(const AFK_SwarmShapeDisplayUnit& _unit)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.push_back(_unit);
}

void AFK_SwarmShapeDisplayQueue::draw(
    AFK_ShaderProgram *shaderProgram,
    AFK_Jigsaw *vapourJigsaw,
    const AFK_3DSwarmShapeBase *baseShape,
    const AFK_ShapeSizes& sSizes)
{
    size_t instanceCount = queue.size();
    if (instanceCount == 0) return;

    if (!tboSamplerLocation)
    {
        jigsawFeatureSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawFeatureTex");
        tboSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
        jigsawPiecePitchLocation = glGetUniformLocation(shaderProgram->program, "JigsawPiecePitch");
        edgeThresholdLocation = glGetUniformLocation(shaderProgram->program, "EdgeThreshold");
    }

    /* Set up the jigsaw. */
    Vec3<float> vapourJigsawPiecePitchSTR = vapourJigsaw->getPiecePitchSTR();
    glUniform3fv(jigsawPiecePitchLocation, 1, &vapourJigsawPiecePitchSTR.v[0]);

    glActiveTexture(GL_TEXTURE0);
    vapourJigsaw->bindTexture(0);
    glUniform1i(jigsawFeatureSamplerLocation, 0);

    /* Set up the distant shape texbuf. */
    glActiveTexture(GL_TEXTURE1);
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
        queue.size() * AFK_SWARM_SHAPE_DISPLAY_UNIT_SIZE,
        queue.data(),
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("distant shape display queue texBuffer");
    glUniform1i(tboSamplerLocation, 1);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif

    baseShape->draw(instanceCount);

    glBindTexture(GL_TEXTURE_3D, 0);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

bool AFK_SwarmShapeDisplayQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return queue.empty();
}

void AFK_SwarmShapeDisplayQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.clear();
}
