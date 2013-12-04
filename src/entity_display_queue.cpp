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

#include "debug.hpp"
#include "display.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"

AFK_EntityDisplayUnit::AFK_EntityDisplayUnit(
    const Mat4<float>& _transform,
    const Vec4<float>& _location,
    const Vec3<float>& _vapourJigsawPieceSTR,
    const Vec2<float>& _edgeJigsawPieceST):
        transform(_transform),
        location(_location),
        vapourJigsawPieceSTR(_vapourJigsawPieceSTR),
        edgeJigsawPieceST(_edgeJigsawPieceST)
{
}

AFK_EntityDisplayQueue::AFK_EntityDisplayQueue():
    buf(0),
    vapourJigsawPiecePitchLocation(0),
    edgeJigsawPiecePitchLocation(0),
    jigsawDensityTexSamplerLocation(0),
    jigsawNormalTexSamplerLocation(0),
    jigsawOverlapTexSamplerLocation(0),
    displayTBOSamplerLocation(0)
{
}

AFK_EntityDisplayQueue::~AFK_EntityDisplayQueue()
{
    if (buf)
        glDeleteBuffers(1, &buf);
}

void AFK_EntityDisplayQueue::add(const AFK_EntityDisplayUnit& _unit)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.push_back(_unit);
}

void AFK_EntityDisplayQueue::draw(
    AFK_ShaderProgram *shaderProgram,
    AFK_Jigsaw *vapourJigsaw,
    AFK_Jigsaw *edgeJigsaw,
    const AFK_3DEdgeShapeBase *baseShape,
    const AFK_ShapeSizes& sSizes)
{
    size_t instanceCount = queue.size();
    if (instanceCount == 0) return;

    if (!displayTBOSamplerLocation)
    {
        vapourJigsawPiecePitchLocation = glGetUniformLocation(shaderProgram->program, "VapourJigsawPiecePitch");
        edgeJigsawPiecePitchLocation = glGetUniformLocation(shaderProgram->program, "EdgeJigsawPiecePitch");
        jigsawDensityTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawDensityTex");
        jigsawNormalTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawNormalTex");
        jigsawOverlapTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawOverlapTex");
        displayTBOSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
    }

    /* Jigsaw initialisation. */
    Vec3<float> vapourJigsawPiecePitchSTR = vapourJigsaw->getPiecePitchSTR();
    Vec2<float> edgeJigsawPiecePitchST = edgeJigsaw->getPiecePitchST();

    glUniform3fv(vapourJigsawPiecePitchLocation, 1, &vapourJigsawPiecePitchSTR.v[0]);

    /* Subtlety: that's a piece pitch for the big 3x2 pieces that
     * correspond to each cube.  However, 0-1 texture co-ordinates in
     * the base shape are for each *face*, so I need to divide the
     * pitch down:
     */
    edgeJigsawPiecePitchST = afk_vec2<float>(
        edgeJigsawPiecePitchST.v[0] / 3.0f,
        edgeJigsawPiecePitchST.v[1] / 2.0f);
    glUniform2fv(edgeJigsawPiecePitchLocation, 1, &edgeJigsawPiecePitchST.v[0]);

    glActiveTexture(GL_TEXTURE0);
    vapourJigsaw->bindTexture(0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawDensityTexSamplerLocation, 0);

    glActiveTexture(GL_TEXTURE1);
    vapourJigsaw->bindTexture(1);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawNormalTexSamplerLocation, 1);

    glActiveTexture(GL_TEXTURE2);
    edgeJigsaw->bindTexture(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(jigsawOverlapTexSamplerLocation, 2);
    
    /* Set up the entity display texbuf. */
    glActiveTexture(GL_TEXTURE3);
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
        queue.size() * ENTITY_DISPLAY_UNIT_SIZE,
        queue.data(),
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("entity display queue texBuffer")
    glUniform1i(displayTBOSamplerLocation, 3);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif

    baseShape->draw(instanceCount);
}

bool AFK_EntityDisplayQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return queue.empty();
}

void AFK_EntityDisplayQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.clear();
}

