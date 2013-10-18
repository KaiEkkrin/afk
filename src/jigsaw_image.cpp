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
#include <cmath>
#include <sstream>

#include "computer.hpp"
#include "core.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "jigsaw_image.hpp"


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


/* AFK_JigsawImage implementation */

void AFK_JigsawImage::getClChangeData(
    const std::vector<AFK_JigsawCuboid>& drawCuboids,
    cl_command_queue q,
    const std::vector<cl_event>& eventWaitList)
{
    size_t pieceSizeInBytes = format.texelSize * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2];

    unsigned int changeEvent = 0;
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < drawCuboids.size(); ++cI)
    {
        size_t cuboidSizeInBytes = pieceSizeInBytes *
            drawCuboids[cI].rows *
            drawCuboids[cI].columns.load() *
            drawCuboids[cI].slices;
  
        if (cuboidSizeInBytes == 0) continue;
  
        size_t origin[3];
        size_t region[3];
  
        origin[0] = drawCuboids[cI].r * pieceSize.v[0];
        origin[1] = drawCuboids[cI].c * pieceSize.v[1];
        origin[2] = drawCuboids[cI].s * pieceSize.v[2];
  
        region[0] = drawCuboids[cI].rows * pieceSize.v[0];
        region[1] = drawCuboids[cI].columns.load() * pieceSize.v[1];
        region[2] = drawCuboids[cI].slices * pieceSize.v[2];

        changeData.resize(changeDataOffset + cuboidSizeInBytes);
        changeEvents.resize(changeEvent + 1);
  
        AFK_CLCHK(clEnqueueReadImage(
            q, clTex, CL_FALSE, origin, region, 0, 0, &changeData[changeDataOffset],
                eventWaitList.size(), &eventWaitList[0], &changeEvents[changeEvent]))
        ++changeEvent;
        changeDataOffset += cuboidSizeInBytes;
    }
}

void AFK_JigsawImage::getClChangeDataFake3D(
    const std::vector<AFK_JigsawCuboid>& drawCuboids,
    cl_command_queue q,
    const std::vector<cl_event>& eventWaitList)
{
    size_t pieceSliceSizeInBytes = format.texelSize * pieceSize.v[0] * pieceSize.v[1];

    unsigned int changeEvent = 0;
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < drawCuboids.size(); ++cI)
    {
        size_t cuboidSliceSizeInBytes = pieceSliceSizeInBytes *
            drawCuboids[cI].rows *
            drawCuboids[cI].columns.load();

        if (cuboidSliceSizeInBytes == 0) continue;

        /* I need to individually transfer each cuboid slice: */
        for (int slice = 0; slice < drawCuboids[cI].slices; ++slice)
        {
            /* ... and within that, I also need a separate transfer
             * for each piece slice
             */
            for (int pieceSlice = 0; pieceSlice < pieceSize.v[2]; ++pieceSlice)
            {
                size_t origin[3];
                size_t region[3];

                Vec2<int> clOrigin = fake3D.fake3DTo2D(afk_vec3<int>(
                    drawCuboids[cI].r * pieceSize.v[0],
                    drawCuboids[cI].c * pieceSize.v[1],
                    (drawCuboids[cI].s + slice) * pieceSize.v[2] + pieceSlice));
                origin[0] = clOrigin.v[0];
                origin[1] = clOrigin.v[1];
                origin[2] = 0;

                region[0] = drawCuboids[cI].rows * pieceSize.v[0];
                region[1] = drawCuboids[cI].columns.load() * pieceSize.v[1];
                region[2] = 1;

                changeData.resize(changeDataOffset + cuboidSliceSizeInBytes);
                changeEvents.resize(changeEvent + 1);

                AFK_CLCHK(clEnqueueReadImage(
                    q, clTex, CL_FALSE, origin, region, 0, 0, &changeData[changeDataOffset],
                        eventWaitList.size(), &eventWaitList[0], &changeEvents[changeEvent]))
                ++changeEvent;
                changeDataOffset += cuboidSliceSizeInBytes;
            }
        }
    }
}

void AFK_JigsawImage::putClChangeData(const std::vector<AFK_JigsawCuboid>& drawCuboids)
{
    size_t pieceSizeInBytes = format.texelSize * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2];
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < drawCuboids.size(); ++cI)
    {
        size_t cuboidSizeInBytes = pieceSizeInBytes *
            drawCuboids[cI].rows *
            drawCuboids[cI].columns.load() *
            drawCuboids[cI].slices;

        if (cuboidSizeInBytes == 0) continue;

        switch (texTarget)
        {
        case GL_TEXTURE_2D:
            glTexSubImage2D(
                texTarget, 0,
                drawCuboids[cI].r * pieceSize.v[0],
                drawCuboids[cI].c * pieceSize.v[1],
                drawCuboids[cI].rows * pieceSize.v[0],
                drawCuboids[cI].columns.load() * pieceSize.v[1],
                format.glFormat,
                format.glDataType,
                &changeData[changeDataOffset]);
            break;

        case GL_TEXTURE_3D:
            glTexSubImage3D(
                texTarget, 0,
                drawCuboids[cI].r * pieceSize.v[0],
                drawCuboids[cI].c * pieceSize.v[1],
                drawCuboids[cI].s * pieceSize.v[2],
                drawCuboids[cI].rows * pieceSize.v[0],
                drawCuboids[cI].columns.load() * pieceSize.v[1],
                drawCuboids[cI].slices * pieceSize.v[2],
                format.glFormat,
                format.glDataType,
                &changeData[changeDataOffset]);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }

        changeDataOffset += cuboidSizeInBytes;
    }
}

AFK_JigsawImage::AFK_JigsawImage(
    cl_context ctxt,
    const Vec3<int>& _pieceSize,
    const Vec3<int>& _jigsawSize,
    const AFK_JigsawFormatDescriptor& _format,
    GLuint _texTarget,
    const AFK_JigsawFake3DDescriptor& _fake3D,
    enum AFK_JigsawBufferUsage _bufferUsage):
        glTex(0),
        clTex(0),
        format(_format),
        texTarget(_texTarget),
        fake3D(_fake3D),
        clImageType((texTarget == GL_TEXTURE_3D && !_fake3D.getUseFake3D()) ?
            CL_MEM_OBJECT_IMAGE3D : CL_MEM_OBJECT_IMAGE2D),
        pieceSize(_pieceSize),
        bufferUsage(_bufferUsage)
{
    cl_int error;

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

#ifndef AFK_OPENCL11
        if (afk_core.computer->testVersion(1, 2))
        {
            clTex = clCreateImage(
                ctxt,
                CL_MEM_READ_WRITE,
                &format.clFormat,
                &imageDesc,
                nullptr,
                &error);
        }
        else
#endif /* AFK_OPENCL11 */
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            switch (clImageType)
            {
            case CL_MEM_OBJECT_IMAGE2D:
                clTex = clCreateImage2D(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &format.clFormat,
                    imageDesc.image_width,
                    imageDesc.image_height,
                    imageDesc.image_row_pitch,
                    nullptr,
                    &error);
                break;

            case CL_MEM_OBJECT_IMAGE3D:
                clTex = clCreateImage3D(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &format.clFormat,
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
    }

    if (bufferUsage != AFK_JIGSAW_BU_CL_ONLY)
    {
        glGenTextures(1, &glTex);
        glBindTexture(texTarget, glTex);

        switch (texTarget)
        {
        case GL_TEXTURE_2D:
            glTexStorage2D(
                texTarget,
                1,
                format.glInternalFormat,
                pieceSize.v[0] * _jigsawSize.v[0],
                pieceSize.v[1] * _jigsawSize.v[1]);
            break;

        case GL_TEXTURE_3D:
            glTexStorage3D(
                texTarget,
                1,
                format.glInternalFormat,
                pieceSize.v[0] * _jigsawSize.v[0],
                pieceSize.v[1] * _jigsawSize.v[1],
                pieceSize.v[2] * _jigsawSize.v[2]);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }
        AFK_GLCHK("AFK_JigSaw texStorage")

        if (bufferUsage == AFK_JIGSAW_BU_CL_GL_SHARED)
        {
#ifndef AFK_OPENCL11
            if (afk_core.computer->testVersion(1, 2))
            {
                clTex = clCreateFromGLTexture(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    texTarget,
                    0,
                    glTex,
                    &error);
            }
            else
#endif /* AFK_OPENCL11 */
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                switch (texTarget)
                {
                case GL_TEXTURE_2D:
                    clTex = clCreateFromGLTexture2D(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        texTarget,
                        0,
                        glTex,
                        &error);           
                    break;

                case GL_TEXTURE_3D:
                    clTex = clCreateFromGLTexture3D(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        texTarget,
                        0,
                        glTex,
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

AFK_JigsawImage::~AFK_JigsawImage()
{
    if (clTex) clReleaseMemObject(clTex);
    for (auto ev : changeEvents)
        if (ev) clReleaseEvent(ev);
    if (glTex) glDeleteTextures(1, &glTex);
}

cl_mem AFK_JigsawImage::acquireForCl(cl_context ctxt, cl_command_queue q, std::vector<cl_event>& o_events)
{
    switch (bufferUsage)
    {
    case AFK_JIGSAW_BU_CL_GL_SHARED:
        cl_event acquireEvent;
        AFK_CLCHK(clEnqueueAcquireGLObjects(q, 1, &clTex, changeEvents.size(), &changeEvents[0], &acquireEvent))
        for (auto ev : changeEvents)
        {
            AFK_CLCHK(clReleaseEvent(ev))
        }
        changeEvents.clear();
        o_events.push_back(acquireEvent);
        break;

    default:
        /* Make sure the change state is reset so that I can start
         * accumulating new changes
         * If there are leftover events the caller needs to wait
         * for them ...
         */
        for (auto ev : changeEvents)
        {
            AFK_CLCHK(clRetainEvent(ev))
            o_events.push_back(ev);
        }
        break;
    }

    return clTex;
}

void AFK_JigsawImage::releaseFromCl(const std::vector<AFK_JigsawCuboid>& drawCuboids, cl_command_queue q, const std::vector<cl_event>& eventWaitList)
{
    std::vector<cl_event> allWaitList;

    switch (bufferUsage)
    {
    case AFK_JIGSAW_BU_CL_GL_COPIED:
        /* Make sure that anything pending with that change data has been
         * finished up because I'm about to re-use the buffer.
         */
        if (changeEvents.size() > 0)
        {
            AFK_CLCHK(clWaitForEvents(changeEvents.size(), &changeEvents[0]))
            for (auto ev : changeEvents)
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
            getClChangeDataFake3D(drawCuboids, q, eventWaitList);
        else
            getClChangeData(drawCuboids, q, eventWaitList);
        break;

    case AFK_JIGSAW_BU_CL_GL_SHARED:
        /* A release here is contingent on any old change events
         * being finished, of course.
         */
        allWaitList.reserve(eventWaitList.size() + changeEvents.size());
        for (auto ev : eventWaitList)
        {
            AFK_CLCHK(clRetainEvent(ev))
            allWaitList.push_back(ev);
        }
        std::copy(changeEvents.begin(), changeEvents.end(), allWaitList.end());

        changeEvents.resize(1);
        AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clTex, allWaitList.size(), &allWaitList[0], &changeEvents[0]))
        for (auto aw : allWaitList)
        {
            AFK_CLCHK(clReleaseEvent(aw))
        }
        allWaitList.clear();
        break;

    default:
        /* The supplied events get retained and fed into the
         * change event list, so that they can be waited for
         * upon any subsequent acquire.
         */
        for (auto ev : eventWaitList)
        {
            AFK_CLCHK(clRetainEvent(ev))
            changeEvents.push_back(ev);
        }
        break;
    }
}

void AFK_JigsawImage::bindTexture(const std::vector<AFK_JigsawCuboid>& drawCuboids)
{
    glBindTexture(texTarget, glTex);

    if (bufferUsage == AFK_JIGSAW_BU_CL_GL_COPIED)
    {
        /* Wait for the change readback to be finished. */
        if (changeEvents.size() == 0) return;

        AFK_CLCHK(clWaitForEvents(changeEvents.size(), &changeEvents[0]))
        for (unsigned int e = 0; e < changeEvents.size(); ++e)
        {
            AFK_CLCHK(clReleaseEvent(changeEvents[e]))
        }

        changeEvents.clear();

        /* Push all the changed pieces into the GL texture. */
        putClChangeData(drawCuboids);
    }
}

void AFK_JigsawImage::waitForAll(void)
{
    if (changeEvents.size() > 0)
    {
        AFK_CLCHK(clWaitForEvents(changeEvents.size(), &changeEvents[0]))
        for (auto ev : changeEvents)
        {
            AFK_CLCHK(clReleaseEvent(ev))
        }
        changeEvents.clear();
    }
}

