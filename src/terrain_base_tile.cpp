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

#include "display.hpp"
#include "terrain_base_tile.hpp"

#define RESTART_INDEX 65535

AFK_TerrainBaseTileVertex::AFK_TerrainBaseTileVertex(
    const Vec3<float>& _location,
    const Vec2<float>& _tileCoord):
        location(_location), tileCoord(_tileCoord)
{
}

AFK_TerrainBaseTile::AFK_TerrainBaseTile(const AFK_LandscapeSizes& lSizes):
    bufs(nullptr)
{
    for (unsigned int x = 0; x < lSizes.vDim; ++x)
    {
        for (unsigned int z = 0; z < lSizes.vDim; ++z)
        {
            float xCoord = (float)x / (float)lSizes.pointSubdivisionFactor;
            float zCoord = (float)z / (float)lSizes.pointSubdivisionFactor;

            /* The texture co-ordinates for each jigsaw piece range
             * from (0, 1), with tDim texels along each side, including
             * the padding all the way round.  Therefore, to access the
             * correct texels, I need to skip the padding, like so:
             */
            float xTex = ((float)x - (float)lSizes.tDimStart + AFK_SAMPLE_WIGGLE) / (float)lSizes.tDim;
            float zTex = ((float)z - (float)lSizes.tDimStart + AFK_SAMPLE_WIGGLE) / (float)lSizes.tDim;

            vertices.push_back(AFK_TerrainBaseTileVertex(
                afk_vec3<float>(xCoord, 0.0f, zCoord),
                afk_vec2<float>(xTex, zTex)));
        }
    }
    
    for (unsigned short x = 0; x < lSizes.pointSubdivisionFactor; ++x)
    {
        for (unsigned short z = 0; z < lSizes.pointSubdivisionFactor; ++z)
        {
            unsigned short i_r1c1 = x * lSizes.vDim + z;
            unsigned short i_r2c1 = (x + 1) * lSizes.vDim + z;
            unsigned short i_r1c2 = x * lSizes.vDim + (z + 1);
            unsigned short i_r2c2 = (x + 1) * lSizes.vDim + (z + 1);

            indices.push_back(i_r1c1);
            indices.push_back(i_r1c2);
            indices.push_back(i_r2c1);
            indices.push_back(i_r2c2);
            indices.push_back(RESTART_INDEX);
        }
    }
}

AFK_TerrainBaseTile::~AFK_TerrainBaseTile()
{
    if (bufs)
    {
        glDeleteBuffers(2, &bufs[0]);
        delete[] bufs;
    }
}

void AFK_TerrainBaseTile::initGL()
{
    bool needBufferPush = (bufs == nullptr);
    if (needBufferPush)
    {
        bufs = new GLuint[2];
        glGenBuffers(2, bufs);
    }

    glBindBuffer(GL_ARRAY_BUFFER, bufs[0]);
    if (needBufferPush)
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * AFK_TER_BASE_VERTEX_SIZE, &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufs[1]);
    if (needBufferPush)
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, AFK_TER_BASE_VERTEX_SIZE, 0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, AFK_TER_BASE_VERTEX_SIZE, (GLvoid *)sizeof(Vec3<float>));

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(RESTART_INDEX);
}

void AFK_TerrainBaseTile::draw(size_t instanceCount) const
{
    glDrawElementsInstanced(GL_TRIANGLE_STRIP, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_SHORT, 0, static_cast<GLsizei>(instanceCount));
    AFK_GLCHK("terrain draw");
}

void AFK_TerrainBaseTile::teardownGL(void) const
{
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
}

