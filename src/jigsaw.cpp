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

#include <cassert>
#include <climits>
#include <cmath>
#include <cstring>
#include <iostream>

#include "computer.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "jigsaw.hpp"


#define GRAB_DEBUG 0
#define CUBOID_DEBUG 0


/* AFK_JigsawFormatDescriptor implementation */

AFK_JigsawFormatDescriptor::AFK_JigsawFormatDescriptor(enum AFK_JigsawFormat e)
{
    switch (e)
    {
    case AFK_JIGSAW_UINT32:
        glInternalFormat                    = GL_R32UI;
        glFormat                            = GL_RED_INTEGER;
        glDataType                          = GL_UNSIGNED_INT;
        clFormat.image_channel_order        = CL_R;
        clFormat.image_channel_data_type    = CL_UNSIGNED_INT32;
        texelSize                           = sizeof(uint32_t);
        break;

    case AFK_JIGSAW_2UINT32:
        glInternalFormat                    = GL_RG32UI;
        glFormat                            = GL_RG_INTEGER;
        glDataType                          = GL_UNSIGNED_INT;
        clFormat.image_channel_order        = CL_RG;
        clFormat.image_channel_data_type    = CL_UNSIGNED_INT32;
        texelSize                           = sizeof(unsigned int) * 2;
        break;

    case AFK_JIGSAW_FLOAT32:
        glInternalFormat                    = GL_R32F;
        glFormat                            = GL_RED;
        glDataType                          = GL_FLOAT;
        clFormat.image_channel_order        = CL_R;
        clFormat.image_channel_data_type    = CL_FLOAT;
        texelSize                           = sizeof(float);
        break;

    /* Note that OpenCL doesn't support 3-byte formats.
     * TODO I'd love to manage to make the packed formats (below) line up
     * between OpenCL and OpenGL, but right now I can't.  It looks like the
     * internal representations might be different (ewww)
     * So for RGB textures, you're stuck with using
     * 4FLOAT8_UNORM and 4FLOAT8_SNORM for now.
     */

    case AFK_JIGSAW_555A1:
        glInternalFormat                    = GL_RGB5;
        glFormat                            = GL_RGB;
        glDataType                          = GL_UNSIGNED_SHORT_5_5_5_1;
        clFormat.image_channel_order        = CL_RGB;
        clFormat.image_channel_data_type    = CL_UNORM_SHORT_555;
        texelSize                           = sizeof(uint16_t);
        break;

    case AFK_JIGSAW_101010A2:
        glInternalFormat                    = GL_RGB10;
        glFormat                            = GL_RGB;
        glDataType                          = GL_UNSIGNED_INT_10_10_10_2;
        clFormat.image_channel_order        = CL_RGB;
        clFormat.image_channel_data_type    = CL_UNORM_INT_101010;
        texelSize                           = sizeof(uint32_t);
        break;

    case AFK_JIGSAW_4FLOAT8_UNORM:
        glInternalFormat                    = GL_RGBA8;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_UNSIGNED_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_UNORM_INT8;
        texelSize                           = sizeof(uint8_t) * 4;
        break;

    /* TODO cl_gl sharing seems to barf with this one ... */
    case AFK_JIGSAW_4FLOAT8_SNORM:
        glInternalFormat                    = GL_RGBA8_SNORM;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_SNORM_INT8;
        texelSize                           = sizeof(uint8_t) * 4;
        break;

    case AFK_JIGSAW_4FLOAT32:
        glInternalFormat                    = GL_RGBA32F;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_FLOAT;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_FLOAT;
        texelSize                           = sizeof(float) * 4;
        break;

    default:
        {
            std::ostringstream ss;
            ss << "Unrecognised jigsaw format: " << e;
            throw AFK_Exception(ss.str());
        }
    }
}

AFK_JigsawFormatDescriptor::AFK_JigsawFormatDescriptor(
    const AFK_JigsawFormatDescriptor& _fd)
{
    glInternalFormat                    = _fd.glInternalFormat;
    glFormat                            = _fd.glFormat;
    glDataType                          = _fd.glDataType;
    clFormat.image_channel_order        = _fd.clFormat.image_channel_order;
    clFormat.image_channel_data_type    = _fd.clFormat.image_channel_data_type;
    texelSize                           = _fd.texelSize;
}


/* AFK_JigsawPiece implementation */

AFK_JigsawPiece::AFK_JigsawPiece():
    u(INT_MIN), v(INT_MIN), w(INT_MIN), puzzle(INT_MIN)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(int _u, int _v, int _w, int _puzzle):
    u(_u), v(_v), w(_w), puzzle(_puzzle)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(const Vec3<int>& _piece, int _puzzle):
    u(_piece.v[0]), v(_piece.v[1]), w(_piece.v[2]), puzzle(_puzzle)
{
}

bool AFK_JigsawPiece::operator==(const AFK_JigsawPiece& other) const
{
    return (u == other.u && v == other.v && w == other.w && puzzle == other.puzzle);
}

bool AFK_JigsawPiece::operator!=(const AFK_JigsawPiece& other) const
{
    return (u != other.u || v != other.v || w != other.w || puzzle != other.puzzle);
}

size_t hash_value(const AFK_JigsawPiece& jigsawPiece)
{
    /* Inspired by hash_value(const AFK_Cell&), just in case I
     * need to use this in anger
     */
    size_t hash = 0;
    boost::hash_combine(hash, jigsawPiece.u * 0x0000c00180030006ll);
    boost::hash_combine(hash, jigsawPiece.v * 0x00180030006000c0ll);
    boost::hash_combine(hash, jigsawPiece.w * 0x00030006000c0018ll);
    boost::hash_combine(hash, jigsawPiece.puzzle * 0x006000c001800300ll);
    return hash;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece)
{
    return os << "JigsawPiece(u=" << std::dec << piece.u << ", v=" << piece.v << ", w=" << piece.w << ", puzzle=" << piece.puzzle << ")";
}


/* AFK_JigsawFake3DDescriptor implementation */

AFK_JigsawFake3DDescriptor::AFK_JigsawFake3DDescriptor():
    useFake3D(false)
{
}

AFK_JigsawFake3DDescriptor::AFK_JigsawFake3DDescriptor(
    bool _useFake3D, const Vec3<int>& _fakeSize):
        fakeSize(_fakeSize), useFake3D(_useFake3D)
{
    float fMult = ceil(sqrt((float)fakeSize.v[2]));
    mult = (int)fMult;
}

AFK_JigsawFake3DDescriptor::AFK_JigsawFake3DDescriptor(
    const AFK_JigsawFake3DDescriptor& _fake3D):
        fakeSize(_fake3D.fakeSize),
        mult(_fake3D.mult),
        useFake3D(_fake3D.useFake3D)
{
}

AFK_JigsawFake3DDescriptor AFK_JigsawFake3DDescriptor::operator=(
    const AFK_JigsawFake3DDescriptor& _fake3D)
{
    fakeSize    = _fake3D.fakeSize;
    mult        = _fake3D.mult;
    useFake3D   = _fake3D.useFake3D;
    return *this;
}

bool AFK_JigsawFake3DDescriptor::getUseFake3D(void) const
{
    return useFake3D;
}

Vec3<int> AFK_JigsawFake3DDescriptor::get2DSize(void) const
{
    assert(useFake3D);
    return afk_vec3<int>(
        fakeSize.v[0] * mult,
        fakeSize.v[1] * mult,
        1);
}

Vec3<int> AFK_JigsawFake3DDescriptor::getFakeSize(void) const
{
    assert(useFake3D);
    return fakeSize;
}

int AFK_JigsawFake3DDescriptor::getMult(void) const
{
    assert(useFake3D);
    return mult;
}

Vec2<int> AFK_JigsawFake3DDescriptor::fake3DTo2D(const Vec3<int>& _fake) const
{
    assert(useFake3D);
    return afk_vec2<int>(
        _fake.v[0] + fakeSize.v[0] * (_fake.v[2] % mult),
        _fake.v[1] + fakeSize.v[1] * (_fake.v[2] / mult));
}

Vec3<int> AFK_JigsawFake3DDescriptor::fake3DFrom2D(const Vec2<int>& _real) const
{
    assert(useFake3D);
    int sFactor = (_real.v[0] / fakeSize.v[0]);
    int tFactor = (_real.v[1] / fakeSize.v[1]);
    return afk_vec3<int>(
        _real.v[0] % fakeSize.v[0],
        _real.v[1] % fakeSize.v[1],
        sFactor + mult * tFactor);
}


/* AFK_JigsawCuboid implementation */

AFK_JigsawCuboid::AFK_JigsawCuboid(int _r, int _c, int _s, int _rows, int _slices):
    r(_r), c(_c), s(_s), rows(_rows), slices(_slices)
{
    columns.store(0);
}

AFK_JigsawCuboid::AFK_JigsawCuboid(const AFK_JigsawCuboid& other):
    r(other.r), c(other.c), s(other.s), rows(other.rows), slices(other.slices)
{
    columns.store(other.columns.load());
}

AFK_JigsawCuboid AFK_JigsawCuboid::operator=(const AFK_JigsawCuboid& other)
{
    r = other.r;
    c = other.c;
    s = other.s;
    rows = other.rows;
    columns.store(other.columns.load());
    slices = other.slices;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& sr)
{
    os << "JigsawCuboid(";
    os << "r=" << std::dec << sr.r;
    os << ", c=" << sr.c;
    os << ", s=" << sr.s;
    os << ", rows=" << sr.rows;
    os << ", columns=" << sr.columns.load();
    os << ", slices=" << sr.slices << ")";
    return os;
}


/* AFK_Jigsaw implementation */

enum AFK_JigsawPieceGrabStatus AFK_Jigsaw::grabPieceFromCuboid(
    AFK_JigsawCuboid& cuboid,
    unsigned int threadId,
    Vec3<int>& o_uvw,
    AFK_Frame *o_timestamp)
{
#if GRAB_DEBUG
    AFK_DEBUG_PRINTL("grabPieceFromCuboid: with cuboid " << cuboid << "( " << cuboid << " and threadId " << threadId)
#endif

    if ((int)threadId >= cuboid.rows)
    {
        /* There aren't enough rows in this cuboid, try the
         * next one.
         */
#if GRAB_DEBUG
        AFK_DEBUG_PRINTL("  out of rows")
#endif
        return AFK_JIGSAW_CUBOID_OUT_OF_SPACE;
    }

    /* If I get here, `threadId' is a valid row within the current
     * cuboid.
     * Let's see if I've got enough columns left...
     */
    int row = (int)threadId + cuboid.r;
    int slice = cuboid.s;
    if (rowUsage[row][slice] < jigsawSize.v[1])
    {
        /* We have!  Grab one. */
        o_uvw = afk_vec3<int>(row, rowUsage[row][slice]++, slice);
        *o_timestamp = rowTimestamp[row][slice];
        piecesGrabbed.fetch_add(1);

        /* If I just gave the cuboid another column, update its columns
         * field to match
         * (I can do this atomically and don't need a lock)
         */
        int cuboidColumns = o_uvw.v[1] - cuboid.c;
        cuboid.columns.compare_exchange_strong(cuboidColumns, cuboidColumns + 1);

#if GRAB_DEBUG
        AFK_DEBUG_PRINTL("  grabbed: " << o_uvw)
#endif

        return AFK_JIGSAW_CUBOID_GRABBED;
    }
    else
    {
        /* We ran out of columns, you need to start a new cuboid.
         */
#if GRAB_DEBUG
        AFK_DEBUG_PRINTL("  out of columns")
#endif
        return AFK_JIGSAW_CUBOID_OUT_OF_COLUMNS;
    }
}

void AFK_Jigsaw::pushNewCuboid(AFK_JigsawCuboid cuboid)
{
    cuboids[updateCs].push_back(cuboid);
    for (int row = cuboid.r; row < (cuboid.r + cuboid.rows); ++row)
    {
        for (int slice = cuboid.s; slice < (cuboid.s + cuboid.slices); ++slice)
        {
            rowUsage[row][slice] = cuboid.c;
        }
    }

#if CUBOID_DEBUG
    AFK_DEBUG_PRINTL("  new cuboid at: " << cuboid)
#endif
}

bool AFK_Jigsaw::startNewCuboid(const AFK_JigsawCuboid& lastCuboid, bool startNewRow)
{
#if CUBOID_DEBUG
    AFK_DEBUG_PRINTL("startNewCuboid: starting new cuboid after " << lastCuboid)
#endif

    /* Update the column counts with the last cuboid's. */
    columnCounts.push(lastCuboid.columns.load());

    /* Cuboids are always the same size right now, which makes things easier: */
    int newRowCount = concurrency;
    int newSliceCount = 1;

    int nextFreeColumn = lastCuboid.c + lastCuboid.columns.load();

    if (startNewRow ||
        newRowCount > (jigsawSize.v[1] - nextFreeColumn) ||
        columnCounts.get() > (jigsawSize.v[1] - nextFreeColumn))
    {
        /* Find a place to start a new cuboid.
         * If I can go one row up, use that:
         */
        int newRow, newSlice;
        if ((lastCuboid.r + lastCuboid.rows + newRowCount) <= jigsawSize.v[0])
        {
            /* There is room on the next row up. */
            newRow = lastCuboid.r + lastCuboid.rows;
            newSlice = lastCuboid.s;
        }
        else
        {
            /* Try the next slice along. */
            newRow = 0;
            newSlice = lastCuboid.s + lastCuboid.slices;
            if ((newSlice + newSliceCount) >= jigsawSize.v[2]) newSlice = 0;
        }

        /* Check I'm not running into the sweep position. */
        if ((newSlice <= sweepPosition.v[1] && sweepPosition.v[1] < (newSlice + newSliceCount)) &&
            (newRow <= sweepPosition.v[0] && sweepPosition.v[0] < (newRow + newRowCount)))
        {
            /* Poo. */
#if CUBOID_DEBUG
            AFK_DEBUG_PRINTL("  new cuboid at " << newRow << ", " << newSlice << " ran into sweep position at " << sweepPosition)
#endif
            return false;
        }

        pushNewCuboid(AFK_JigsawCuboid(newRow, 0, newSlice, newRowCount, newSliceCount));
    }
    else
    {
        /* I'm going to pack a new cuboid onto the same row as the existing one.
         * This operation also closes down the last cuboid (if you add columns to
         * it you'll trample the new one).
         */
        pushNewCuboid(AFK_JigsawCuboid(lastCuboid.r, lastCuboid.c + lastCuboid.columns.load(), lastCuboid.s, newRowCount, newSliceCount));
    }

    cuboidsStarted.fetch_add(1);
    return true;
}

int AFK_Jigsaw::roundUpToConcurrency(int r) const
{
    int rmodc = r % concurrency;
    if (rmodc > 0) r += (concurrency - rmodc);
    return r;
}

#define SWEEP_DEBUG 0

Vec2<int> AFK_Jigsaw::getSweepTarget(const Vec2<int>& latest) const
{
    Vec2<int> target;

    if (jigsawSize.v[2] > 1)
    {
        /* Hop across slices. */
        int targetSlice = latest.v[1] + (jigsawSize.v[2] / 8) + 1;
        if (targetSlice >= jigsawSize.v[2]) targetSlice -= jigsawSize.v[2];
        target = afk_vec2<int>(latest.v[0], targetSlice);
    }
    else
    {
        /* Hop across rows. */
        int targetRow = roundUpToConcurrency(latest.v[0] + (jigsawSize.v[0] / 8));
        if (targetRow >= jigsawSize.v[0]) targetRow -= jigsawSize.v[0];
        target = afk_vec2<int>(targetRow, latest.v[1]);
    }

#if SWEEP_DEBUG
    AFK_DEBUG_PRINTL("sweepTarget: " << sweepPosition << " -> " << target << " (latest: " << latest << ", jigsawSize " << jigsawSize << ", concurrency " << concurrency << ")")
#endif

    return target;
}

void AFK_Jigsaw::sweep(const Vec2<int>& sweepTarget, const AFK_Frame& currentFrame)
{
    while (sweepPosition != sweepTarget)
    {
        /* Sweep the rows up to the top, then flip to the next
         * slice.
         */
        rowTimestamp[sweepPosition.v[0]][sweepPosition.v[1]] = currentFrame;
        ++(sweepPosition.v[0]);
        piecesSwept.fetch_add(jigsawSize.v[0]);

        if (sweepPosition.v[0] == jigsawSize.v[0])
        {
            /* I hit the top of the row, set back down again and
             * shift to the next slice.
             */
            sweepPosition.v[0] = 0;
            ++(sweepPosition.v[1]);
            if (sweepPosition.v[1] == jigsawSize.v[2])
            {
                /* I hit the end of the jigsaw, wrap around. */
                sweepPosition.v[1] = 0;
            }
        }
    }
}

void AFK_Jigsaw::doSweep(const Vec2<int>& nextFreeRow, const AFK_Frame& currentFrame)
{
    /* Let's try to keep the sweep row some way ahead of the
     * next free row.
     */
    if (jigsawSize.v[2] > 2)
    {
        int sweepSliceCmp = (sweepPosition.v[1] < nextFreeRow.v[1] ? (sweepPosition.v[1] + jigsawSize.v[2]) : sweepPosition.v[1]) + 1;
        if ((sweepSliceCmp - nextFreeRow.v[1]) < 2)
        {
            sweep(getSweepTarget(afk_vec2<int>(nextFreeRow.v[0], sweepSliceCmp)), currentFrame);
        }
    }
    else
    {
        int sweepRowCmp = roundUpToConcurrency((sweepPosition.v[0] < nextFreeRow.v[0] ? (sweepPosition.v[0] + jigsawSize.v[0]) : sweepPosition.v[0]));
        if ((sweepRowCmp - nextFreeRow.v[0]) < (jigsawSize.v[0] / (int)concurrency))
        {
            sweep(getSweepTarget(afk_vec2<int>(sweepRowCmp, nextFreeRow.v[1])), currentFrame);
        }
    }
}

void AFK_Jigsaw::getClChangeData(cl_command_queue q, const std::vector<cl_event>& eventWaitList, unsigned int tex)
{
    size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2];

    unsigned int changeEvent = 0;
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < cuboids[drawCs].size(); ++cI)
    {
        size_t cuboidSizeInBytes = pieceSizeInBytes *
            cuboids[drawCs][cI].rows *
            cuboids[drawCs][cI].columns.load() *
            cuboids[drawCs][cI].slices;
  
        if (cuboidSizeInBytes == 0) continue;
  
        size_t origin[3];
        size_t region[3];
  
        origin[0] = cuboids[drawCs][cI].r * pieceSize.v[0];
        origin[1] = cuboids[drawCs][cI].c * pieceSize.v[1];
        origin[2] = cuboids[drawCs][cI].s * pieceSize.v[2];
  
        region[0] = cuboids[drawCs][cI].rows * pieceSize.v[0];
        region[1] = cuboids[drawCs][cI].columns.load() * pieceSize.v[1];
        region[2] = cuboids[drawCs][cI].slices * pieceSize.v[2];

        changeData[tex].resize(changeDataOffset + cuboidSizeInBytes);
        changeEvents[tex].resize(changeEvent + 1);
  
        AFK_CLCHK(clEnqueueReadImage(
            q, clTex[tex], CL_FALSE, origin, region, 0, 0, &changeData[tex][changeDataOffset],
                eventWaitList.size(), &eventWaitList[0], &changeEvents[tex][changeEvent]))
        ++changeEvent;
        changeDataOffset += cuboidSizeInBytes;
    }
}

void AFK_Jigsaw::getClChangeDataFake3D(cl_command_queue q, const std::vector<cl_event>& eventWaitList, unsigned int tex)
{
    size_t pieceSliceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1];

    unsigned int changeEvent = 0;
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < cuboids[drawCs].size(); ++cI)
    {
        size_t cuboidSliceSizeInBytes = pieceSliceSizeInBytes *
            cuboids[drawCs][cI].rows *
            cuboids[drawCs][cI].columns.load();

        if (cuboidSliceSizeInBytes == 0) continue;

        /* I need to individually transfer each cuboid slice: */
        for (int slice = 0; slice < cuboids[drawCs][cI].slices; ++slice)
        {
            /* ... and within that, I also need a separate transfer
             * for each piece slice
             */
            for (int pieceSlice = 0; pieceSlice < pieceSize.v[2]; ++pieceSlice)
            {
                size_t origin[3];
                size_t region[3];

                Vec2<int> clOrigin = fake3D.fake3DTo2D(afk_vec3<int>(
                    cuboids[drawCs][cI].r * pieceSize.v[0],
                    cuboids[drawCs][cI].c * pieceSize.v[1],
                    (cuboids[drawCs][cI].s + slice) * pieceSize.v[2] + pieceSlice));
                origin[0] = clOrigin.v[0];
                origin[1] = clOrigin.v[1];
                origin[2] = 0;

                region[0] = cuboids[drawCs][cI].rows * pieceSize.v[0];
                region[1] = cuboids[drawCs][cI].columns.load() * pieceSize.v[1];
                region[2] = 1;

                changeData[tex].resize(changeDataOffset + cuboidSliceSizeInBytes);
                changeEvents[tex].resize(changeEvent + 1);

                AFK_CLCHK(clEnqueueReadImage(
                    q, clTex[tex], CL_FALSE, origin, region, 0, 0, &changeData[tex][changeDataOffset],
                        eventWaitList.size(), &eventWaitList[0], &changeEvents[tex][changeEvent]))
                ++changeEvent;
                changeDataOffset += cuboidSliceSizeInBytes;
            }
        }
    }
}

void AFK_Jigsaw::putClChangeData(unsigned int tex)
{
    size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2];
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < cuboids[drawCs].size(); ++cI)
    {
        size_t cuboidSizeInBytes = pieceSizeInBytes *
            cuboids[drawCs][cI].rows *
            cuboids[drawCs][cI].columns.load() *
            cuboids[drawCs][cI].slices;

        if (cuboidSizeInBytes == 0) continue;

        switch (texTarget)
        {
        case GL_TEXTURE_2D:
            glTexSubImage2D(
                texTarget, 0,
                cuboids[drawCs][cI].r * pieceSize.v[0],
                cuboids[drawCs][cI].c * pieceSize.v[1],
                cuboids[drawCs][cI].rows * pieceSize.v[0],
                cuboids[drawCs][cI].columns.load() * pieceSize.v[1],
                format[tex].glFormat,
                format[tex].glDataType,
                &changeData[tex][changeDataOffset]);
            break;

        case GL_TEXTURE_3D:
            glTexSubImage3D(
                texTarget, 0,
                cuboids[drawCs][cI].r * pieceSize.v[0],
                cuboids[drawCs][cI].c * pieceSize.v[1],
                cuboids[drawCs][cI].s * pieceSize.v[2],
                cuboids[drawCs][cI].rows * pieceSize.v[0],
                cuboids[drawCs][cI].columns.load() * pieceSize.v[1],
                cuboids[drawCs][cI].slices * pieceSize.v[2],
                format[tex].glFormat,
                format[tex].glDataType,
                &changeData[tex][changeDataOffset]);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }

        changeDataOffset += cuboidSizeInBytes;
    }
}

AFK_Jigsaw::AFK_Jigsaw(
    cl_context ctxt,
    const Vec3<int>& _pieceSize,
    const Vec3<int>& _jigsawSize,
    const AFK_JigsawFormatDescriptor *_format,
    GLuint _texTarget,
    const AFK_JigsawFake3DDescriptor& _fake3D,
    unsigned int _texCount,
    enum AFK_JigsawBufferUsage _bufferUsage,
    unsigned int _concurrency):
        glTex(nullptr),
        format(_format),
        texTarget(_texTarget),
        fake3D(_fake3D),
        clImageType((texTarget == GL_TEXTURE_3D && !_fake3D.getUseFake3D()) ?
            CL_MEM_OBJECT_IMAGE3D : CL_MEM_OBJECT_IMAGE2D),
        texCount(_texCount),
        pieceSize(_pieceSize),
        jigsawSize(_jigsawSize),
        bufferUsage(_bufferUsage),
        concurrency(_concurrency),
        updateCs(0),
        drawCs(1),
        columnCounts(8, 0)
{
    cl_int error;

    /* Check for an unsupported case -- image types clash */
    if (bufferUsage == AFK_JIGSAW_BU_CL_GL_SHARED &&
        fake3D.getUseFake3D())
    {
        throw AFK_Exception("Can't support fake 3D with cl_gl sharing");
    }

    clTex = new cl_mem[texCount];
    if (bufferUsage != AFK_JIGSAW_BU_CL_GL_SHARED)
    {
        /* Do native CL buffering here. */
        cl_image_desc imageDesc;
        memset(&imageDesc, 0, sizeof(cl_image_desc));

        Vec3<int> clImageSize;
        if (fake3D.getUseFake3D())
        {
            clImageSize = fake3D.get2DSize();
        }
        else
        {
            clImageSize = afk_vec3<int>(
                _pieceSize.v[0] * _jigsawSize.v[0],
                _pieceSize.v[1] * _jigsawSize.v[1],
                _pieceSize.v[2] * _jigsawSize.v[2]);
        }

        imageDesc.image_type        = clImageType;
        imageDesc.image_width       = clImageSize.v[0];
        imageDesc.image_height      = clImageSize.v[1];
        imageDesc.image_depth       = clImageSize.v[2];

        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            if (afk_core.computer->testVersion(1, 2))
            {
                clTex[tex] = clCreateImage(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &format[tex].clFormat,
                    &imageDesc,
                    nullptr,
                    &error);
            }
            else
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                switch (clImageType)
                {
                case CL_MEM_OBJECT_IMAGE2D:
                    clTex[tex] = clCreateImage2D(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        &format[tex].clFormat,
                        imageDesc.image_width,
                        imageDesc.image_height,
                        imageDesc.image_row_pitch,
                        nullptr,
                        &error);
                    break;
    
                case CL_MEM_OBJECT_IMAGE3D:
                    clTex[tex] = clCreateImage3D(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        &format[tex].clFormat,
                        imageDesc.image_width,
                        imageDesc.image_height,
                        imageDesc.image_depth,
                        imageDesc.image_row_pitch,
                        imageDesc.image_slice_pitch,
                        nullptr,
                        &error);
                    break;
    
                default:
                    throw AFK_Exception("Unrecognised texTarget");
                }
#pragma GCC diagnostic pop
            }
            AFK_HANDLE_CL_ERROR(error);
        }
    }

    if (bufferUsage != AFK_JIGSAW_BU_CL_ONLY)
    {
        glTex = new GLuint[texCount];
        glGenTextures(texCount, glTex);

        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            glBindTexture(texTarget, glTex[tex]);

            switch (texTarget)
            {
            case GL_TEXTURE_2D:
                glTexStorage2D(
                    texTarget,
                    1,
                    format[tex].glInternalFormat,
                    pieceSize.v[0] * jigsawSize.v[0],
                    pieceSize.v[1] * jigsawSize.v[1]);
                break;

            case GL_TEXTURE_3D:
                glTexStorage3D(
                    texTarget,
                    1,
                    format[tex].glInternalFormat,
                    pieceSize.v[0] * jigsawSize.v[0],
                    pieceSize.v[1] * jigsawSize.v[1],
                    pieceSize.v[2] * jigsawSize.v[2]);
                break;

            default:
                throw AFK_Exception("Unrecognised texTarget");
            }
            AFK_GLCHK("AFK_JigSaw texStorage")

            if (bufferUsage == AFK_JIGSAW_BU_CL_GL_SHARED)
            {
                if (afk_core.computer->testVersion(1, 2))
                {
                    clTex[tex] = clCreateFromGLTexture(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        texTarget,
                        0,
                        glTex[tex],
                        &error);
                }
                else
                {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                    switch (texTarget)
                    {
                    case GL_TEXTURE_2D:
                        clTex[tex] = clCreateFromGLTexture2D(
                            ctxt,
                            CL_MEM_READ_WRITE,
                            texTarget,
                            0,
                            glTex[tex],
                            &error);           
                        break;

                    case GL_TEXTURE_3D:
                        clTex[tex] = clCreateFromGLTexture3D(
                            ctxt,
                            CL_MEM_READ_WRITE,
                            texTarget,
                            0,
                            glTex[tex],
                            &error);           
                        break;

                    default:
                        throw AFK_Exception("Unrecognised texTarget");
                    }
#pragma GCC diagnostic pop
                }
                AFK_HANDLE_CL_ERROR(error);
            }
        }
    }

    /* Now that I've got the textures, fill out the jigsaw state. */
    rowTimestamp = new AFK_Frame*[jigsawSize.v[0]];
    rowUsage = new int*[jigsawSize.v[0]];

    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        rowTimestamp[row] = new AFK_Frame[jigsawSize.v[2]];
        rowUsage[row] = new int[jigsawSize.v[2]];

        for (int slice = 0; slice < jigsawSize.v[2]; ++slice)
        {
            rowTimestamp[row][slice] = AFK_Frame();
            rowUsage[row][slice] = 0;
        }
    }

    sweepPosition = getSweepTarget(afk_vec2<int>(0, 0));

    /* Make a starting update cuboid. */
    if (!startNewCuboid(
        AFK_JigsawCuboid(0, 0, 0, concurrency, 1), false))
    {
        throw AFK_Exception("Cannot make starting cuboid");
    }

    changeData = new std::vector<uint8_t>[texCount];
    changeEvents = new std::vector<cl_event>[texCount];

    piecesGrabbed.store(0);
    cuboidsStarted.store(0);
    piecesSwept.store(0);
}

AFK_Jigsaw::~AFK_Jigsaw()
{
    for (unsigned int tex = 0; tex < texCount; ++tex)
    {
        clReleaseMemObject(clTex[tex]);
        for (unsigned int e = 0; e < changeEvents[tex].size(); ++e)
        {
            if (changeEvents[tex][e])
                clReleaseEvent(changeEvents[tex][e]);
        }
    }

    if (glTex) glDeleteTextures(texCount, glTex);

    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        delete[] rowUsage[row];
        delete[] rowTimestamp[row];
    }

    delete[] rowUsage;
    delete[] rowTimestamp;

    delete[] changeEvents;
    delete[] changeData;
    delete[] clTex;
    if (glTex) delete[] glTex;
}

bool AFK_Jigsaw::grab(unsigned int threadId, Vec3<int>& o_uvw, AFK_Frame *o_timestamp)
{
    bool grabSuccessful = false;
    bool gotUpgradeLock = false;
    if (cuboidMuts[updateCs].try_lock_upgrade()) gotUpgradeLock = true;
    else cuboidMuts[updateCs].lock_shared();

    /* Let's see if I can use an existing cuboid. */
    unsigned int cI;
    for (cI = 0; cI < cuboids[updateCs].size(); ++cI)
    {
        switch (grabPieceFromCuboid(cuboids[updateCs][cI], threadId, o_uvw, o_timestamp))
        {
#if 0
        case AFK_JIGSAW_CUBOID_OUT_OF_SPACE:
            /* This means I'm out of room in the jigsaw. */
            goto grab_return;
#endif

        case AFK_JIGSAW_CUBOID_GRABBED:
            /* I got one! */
            grabSuccessful = true;
            goto grab_return;

        default:
            /* Keep looking. */
            break;
        }
    }

    /* I ran out of cuboids.  Try to start a new one. */
    if (cI == 0) goto grab_return;

    if (!gotUpgradeLock)
    {
        cuboidMuts[updateCs].unlock_shared();
        cuboidMuts[updateCs].lock_upgrade();
        gotUpgradeLock = true;
    }

    cuboidMuts[updateCs].unlock_upgrade_and_lock();

    {
        bool retry = true;
        if (cI == cuboids[updateCs].size())
            retry = startNewCuboid(cuboids[updateCs][cI-1], true);

        cuboidMuts[updateCs].unlock();
        if (!retry) return false;
    }

    return grab(threadId, o_uvw, o_timestamp);

grab_return:
    if (gotUpgradeLock) cuboidMuts[updateCs].unlock_upgrade();
    else cuboidMuts[updateCs].unlock_shared();
    return grabSuccessful;
}

AFK_Frame AFK_Jigsaw::getTimestamp(const AFK_JigsawPiece& piece) const
{
    return rowTimestamp[piece.u][piece.w];
}

unsigned int AFK_Jigsaw::getTexCount(void) const
{
    return texCount;
}

Vec2<float> AFK_Jigsaw::getTexCoordST(const AFK_JigsawPiece& piece) const
{
    return afk_vec2<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1]);
}

Vec3<float> AFK_Jigsaw::getTexCoordSTR(const AFK_JigsawPiece& piece) const
{
    return afk_vec3<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1],
        (float)piece.w / (float)jigsawSize.v[2]);
}

Vec2<float> AFK_Jigsaw::getPiecePitchST(void) const
{
    return afk_vec2<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1]);
}

Vec3<float> AFK_Jigsaw::getPiecePitchSTR(void) const
{
    return afk_vec3<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1],
        1.0f / (float)jigsawSize.v[2]);
}

Vec2<int> AFK_Jigsaw::getFake3D_size(void) const
{
    Vec2<int> size2;

    if (fake3D.getUseFake3D())
    {
        Vec3<int> size = fake3D.getFakeSize();
        size2 = afk_vec2<int>(size.v[0], size.v[1]);
    }
    else
    {
        size2 = afk_vec2<int>(-1, -1);
    }

    return size2;
}

int AFK_Jigsaw::getFake3D_mult(void) const
{
    return fake3D.getUseFake3D() ? fake3D.getMult() : 0;
}

cl_mem *AFK_Jigsaw::acquireForCl(cl_context ctxt, cl_command_queue q, std::vector<cl_event>& o_events)
{
    boost::upgrade_lock<boost::upgrade_mutex> lock(cuboidMuts[drawCs]);

    /* Note that acquireForCl() may be called several times (and so
     * may releaseFromCl() ).  When acquire is called it will
     * retain the events for you; when release is called it will
     * retain any release wait events in the event list for further
     * acquires.
     */

    switch (bufferUsage)
    {
    case AFK_JIGSAW_BU_CL_GL_SHARED:
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            cl_event acquireEvent;
            AFK_CLCHK(clEnqueueAcquireGLObjects(q, 1, &clTex[tex], changeEvents[tex].size(), &changeEvents[tex][0], &acquireEvent))
            for (auto ev : changeEvents[tex])
            {
                AFK_CLCHK(clReleaseEvent(ev))
            }
            changeEvents[tex].clear();
            o_events.push_back(acquireEvent);
        }
        break;

    default:
        /* Make sure the change state is reset so that I can start
         * accumulating new changes
         */
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            /* If there are leftover events the caller needs to wait
             * for them ...
             */
            for (auto ev : changeEvents[tex])
            {
                AFK_CLCHK(clRetainEvent(ev))
                o_events.push_back(ev);
            }
        }
        break;
    }

    return clTex;
}

void AFK_Jigsaw::releaseFromCl(cl_command_queue q, const std::vector<cl_event>& eventWaitList)
{
    boost::upgrade_lock<boost::upgrade_mutex> lock(cuboidMuts[drawCs]);

    std::vector<cl_event> allWaitList;

    switch (bufferUsage)
    {
    case AFK_JIGSAW_BU_CL_GL_COPIED:
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            /* Make sure that anything pending with that change data has been
             * finished up because I'm about to re-use the buffer.
             */
            if (changeEvents[tex].size() > 0)
            {
                AFK_CLCHK(clWaitForEvents(changeEvents[tex].size(), &changeEvents[tex][0]))
                for (auto ev : changeEvents[tex])
                {
                    AFK_CLCHK(clReleaseEvent(ev))
                }
            }

            /* Now, read back all the pieces, appending the data to `changeData'
             * in order.
             * So long as `bindTexture' writes them in the same order everything
             * will be okay!
             */
            if (fake3D.getUseFake3D())
                getClChangeDataFake3D(q, eventWaitList, tex);
            else
                getClChangeData(q, eventWaitList, tex);
        }
        break;

    case AFK_JIGSAW_BU_CL_GL_SHARED:
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            /* A release here is contingent on any old change events
             * being finished, of course.
             */
            allWaitList.reserve(eventWaitList.size() + changeEvents[tex].size());
            for (auto ev : eventWaitList)
            {
                AFK_CLCHK(clRetainEvent(ev))
                allWaitList.push_back(ev);
            }
            std::copy(changeEvents[tex].begin(), changeEvents[tex].end(), allWaitList.end());

            changeEvents[tex].resize(1);
            AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clTex[tex], allWaitList.size(), &allWaitList[0], &changeEvents[tex][0]))
            for (auto aw : allWaitList)
            {
                AFK_CLCHK(clReleaseEvent(aw))
            }
            allWaitList.clear();
        }
        break;

    default:
        /* The supplied events get retained and fed into the
         * change event list, so that they can be waited for
         * upon any subsequent acquire.
         */
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            for (auto ev : eventWaitList)
            {
                AFK_CLCHK(clRetainEvent(ev))
                changeEvents[tex].push_back(ev);
            }
        }
        break;
    }
}

void AFK_Jigsaw::bindTexture(unsigned int tex)
{
    boost::upgrade_lock<boost::upgrade_mutex> lock(cuboidMuts[drawCs]);

    glBindTexture(texTarget, glTex[tex]);

    if (bufferUsage == AFK_JIGSAW_BU_CL_GL_COPIED)
    {
        /* Wait for the change readback to be finished. */
        if (changeEvents[tex].size() == 0) return;

        AFK_CLCHK(clWaitForEvents(changeEvents[tex].size(), &changeEvents[tex][0]))
        for (unsigned int e = 0; e < changeEvents[tex].size(); ++e)
        {
            AFK_CLCHK(clReleaseEvent(changeEvents[tex][e]))
        }

        changeEvents[tex].clear();

        /* Push all the changed pieces into the GL texture. */
        putClChangeData(tex);
    }
}

#define FLIP_DEBUG 0

void AFK_Jigsaw::flipCuboids(const AFK_Frame& currentFrame)
{
    /* Make sure any changes are really, properly finished */
    for (unsigned int tex = 0; tex < texCount; ++tex)
    {
        if (changeEvents[tex].size() > 0)
        {
            AFK_CLCHK(clWaitForEvents(changeEvents[tex].size(), &changeEvents[tex][0]))
            for (auto ev : changeEvents[tex])
            {
                AFK_CLCHK(clReleaseEvent(ev))
            }
            changeEvents[tex].clear();
        }
    }

    boost::upgrade_lock<boost::upgrade_mutex> ulock0(cuboidMuts[0]);
    boost::upgrade_to_unique_lock<boost::upgrade_mutex> utoulock0(ulock0);

    boost::upgrade_lock<boost::upgrade_mutex> ulock1(cuboidMuts[1]);
    boost::upgrade_to_unique_lock<boost::upgrade_mutex> utoulock1(ulock0);

    updateCs = (updateCs == 0 ? 1 : 0);
    drawCs = (drawCs == 0 ? 1 : 0);

    /* Clear out the old draw cuboids, and create a new update
     * cuboid following on from the last one.
     */
    cuboids[updateCs].clear();

    /* Do we have an existing cuboid to continue from? */
    bool continued = false;
    if (cuboids[drawCs].size() > 0)
    {
        /* Sweep in front of the cuboid I'm about to make */
        std::vector<AFK_JigsawCuboid>::reverse_iterator lastCuboid = cuboids[drawCs].rbegin();
        Vec2<int> nextFree = afk_vec2<int>(lastCuboid->r + lastCuboid->rows, lastCuboid->s);
        if (nextFree.v[0] >= jigsawSize.v[0])
        {
            nextFree.v[0] = 0;
            nextFree.v[1] = lastCuboid->s + lastCuboid->slices;
            if (nextFree.v[1] >= jigsawSize.v[2]) nextFree.v[1] = 0;
        }
        doSweep(nextFree, currentFrame);

#if FLIP_DEBUG
        AFK_DEBUG_PRINTL("Flipping from draw cuboid " << *lastCuboid << " (with sweep position " << sweepPosition << ")")
#endif
        continued = startNewCuboid(*lastCuboid, false);
    }
    else
    {
#if FLIP_DEBUG
        AFK_DEBUG_PRINTL("Trying to start fresh cuboid")
#endif
        /* This should be a rare case: I previously ran out of
         * cuboids in this jigsaw.
         * Pick a place to try to start from (this may not succeed).
         * I need to ask for a new row to trigger the eviction stuff.
         */
        continued = startNewCuboid(AFK_JigsawCuboid(0, 0, 0, concurrency, 1), true);

        if (continued)
        {
            /* Now, reset the sweep and sweep across that cuboid. */
            sweepPosition = afk_vec2<int>(0, 0);
            Vec2<int> nextFree = afk_vec2<int>(0, 0);
            doSweep(nextFree, currentFrame);
        }
    }

    /* This shouldn't happen, *but* ... */
    if (!continued) throw AFK_Exception("flipCuboids() failed");
}

void AFK_Jigsaw::printStats(std::ostream& os, const std::string& prefix)
{
    /* TODO Time intervals or something?  (Really the relative numbers are more interesting) */
    std::cout << prefix << "\t: Pieces grabbed:       " << piecesGrabbed.exchange(0) << std::endl;
    std::cout << prefix << "\t: Cuboids started:      " << cuboidsStarted.exchange(0) << std::endl;
    std::cout << prefix << "\t: Pieces swept:         " << piecesSwept.exchange(0) << std::endl;  
}

