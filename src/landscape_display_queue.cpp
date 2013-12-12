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

#include <thread>

#include "display.hpp"
#include "jigsaw.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_tile.hpp"
#include "world.hpp"

AFK_LandscapeDisplayUnit::AFK_LandscapeDisplayUnit() {}

AFK_LandscapeDisplayUnit::AFK_LandscapeDisplayUnit(
    const Vec4<float>& _cellCoord,
    const Vec2<float>& _jigsawPieceST,
    float _yBoundLower,
    float _yBoundUpper):
        cellCoord(_cellCoord), jigsawPieceST(_jigsawPieceST), yBoundLower(_yBoundLower), yBoundUpper(_yBoundUpper)
{
}

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeDisplayUnit& unit)
{
    os << "(LDU: ";
    os << "cellCoord=" << std::dec << unit.cellCoord;
    os << ", jigsawPieceST=" << unit.jigsawPieceST;
    os << ", yBoundLower=" << unit.yBoundLower;
    os << ", yBoundUpper=" << unit.yBoundUpper;
    os << ")";
    return os;
}


AFK_LandscapeDisplayQueue::AFK_LandscapeDisplayQueue():
    buf(0),
    jigsawPiecePitchLocation(0),
    jigsawYDispTexSamplerLocation(0),
    jigsawColourTexSamplerLocation(0),
    jigsawNormalTexSamplerLocation(0),
    displayTBOSamplerLocation(0)
{
}

AFK_LandscapeDisplayQueue::~AFK_LandscapeDisplayQueue()
{
    if (buf) glDeleteBuffers(1, &buf);
}

void AFK_LandscapeDisplayQueue::add(const AFK_LandscapeDisplayUnit& _unit, const AFK_Tile& _tile)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.push_back(_unit);
    landscapeTiles.push_back(_tile);
}

#define COPY_TO_GL_CULLING 1

void AFK_LandscapeDisplayQueue::draw(
    unsigned int threadId,
    AFK_ShaderProgram *shaderProgram,
    AFK_Jigsaw *jigsaw,
    AFK_LANDSCAPE_CACHE *cache,
    const AFK_TerrainBaseTile *baseTile,
    const AFK_LandscapeSizes& lSizes)
{
    /* First, check there's anything to draw at all ... */
#if COPY_TO_GL_CULLING
    bool allChecked = false;
    std::vector<bool> checked;
    checked.reserve(queue.size());
    for (unsigned int i = 0; i < queue.size(); ++i) checked.push_back(false);

    do
    {
        allChecked = true;
        for (unsigned int i = 0; i < queue.size(); ++i)
        {
            if (checked[i]) continue;

            try
            {
                auto claim = cache->get(threadId, landscapeTiles[i]).claimable.claimInplace(threadId, AFK_CL_SHARED);
                if (claim.getShared().realCellWithinYBounds(queue[i].cellCoord))
                {
                    /* I do want to draw this tile. */
                    culledQueue.push_back(queue[i]);
                }

                /* If I got here, the tile has been checked successfully. */
                checked[i] = true;
            }
            catch (AFK_PolymerOutOfRange&) { checked[i] = true; /* Ignore, this one shouldn't be drawn */ }
            catch (AFK_ClaimException&) { allChecked = false; /* Want to retry */ }
        }

        if (!allChecked) std::this_thread::yield(); /* Give things a chance */
    } while (!allChecked);

    size_t instanceCount = culledQueue.size();
#else
    size_t instanceCount = queue.size();
#endif
    if (instanceCount == 0) return;

    /* Check I've initialised the various locations in the
     * shader program that I need
     */
    if (!jigsawPiecePitchLocation)
    {
        jigsawPiecePitchLocation = glGetUniformLocation(shaderProgram->program, "JigsawPiecePitch");
        jigsawYDispTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawYDispTex");
        jigsawColourTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawColourTex");
        jigsawNormalTexSamplerLocation = glGetUniformLocation(shaderProgram->program, "JigsawNormalTex");
        displayTBOSamplerLocation = glGetUniformLocation(shaderProgram->program, "DisplayTBO");
    }

    /* Fill out ye olde uniform variable with the jigsaw
     * piece pitch.
     */
    Vec2<float> jigsawPiecePitchST = jigsaw->getPiecePitchST();
    glUniform2fv(jigsawPiecePitchLocation, 1, &jigsawPiecePitchST.v[0]);

    /* The first texture is the jigsaw Y-displacement */
    glActiveTexture(GL_TEXTURE0);
    jigsaw->bindTexture(0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glUniform1i(jigsawYDispTexSamplerLocation, 0);

    /* The second texture is the jigsaw colour */
    glActiveTexture(GL_TEXTURE1);
    jigsaw->bindTexture(1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawColourTexSamplerLocation, 1);

    /* The third texture is the jigsaw normal */
    glActiveTexture(GL_TEXTURE2);
    jigsaw->bindTexture(2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUniform1i(jigsawNormalTexSamplerLocation, 2);

    /* The fourth texture is the landscape display texbuf,
     * which explains to the vertex shader which tile it's
     * drawing and where in the jigsaw to look.
     */
    glActiveTexture(GL_TEXTURE3);
    if (!buf) glGenBuffers(1, &buf);
    glBindBuffer(GL_TEXTURE_BUFFER, buf);
    glBufferData(
        GL_TEXTURE_BUFFER,
#if COPY_TO_GL_CULLING
        culledQueue.size() * sizeof(AFK_LandscapeDisplayUnit),
        &culledQueue[0],
#else
        queue.size() * sizeof(AFK_LandscapeDisplayUnit),
        &queue[0],
#endif
        GL_STREAM_DRAW);
    glTexBuffer(
        GL_TEXTURE_BUFFER,
        GL_RGBA32F,
        buf);
    AFK_GLCHK("landscape display queue texBuffer")
    glUniform1i(displayTBOSamplerLocation, 3);

#if AFK_GL_DEBUG
    shaderProgram->Validate();
#endif

    baseTile->draw(instanceCount);
}

bool AFK_LandscapeDisplayQueue::empty(void)
{
    std::unique_lock<std::mutex> lock(mut);

    return queue.empty();
}

void AFK_LandscapeDisplayQueue::clear(void)
{
    std::unique_lock<std::mutex> lock(mut);

    queue.clear();
    landscapeTiles.clear();
    culledQueue.clear();
}

