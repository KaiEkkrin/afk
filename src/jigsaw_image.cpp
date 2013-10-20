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


/* Various stringifies */

std::ostream& operator<<(std::ostream& os, const AFK_JigsawBufferUsage bufferUsage)
{
    switch (bufferUsage)
    {
    case AFK_JigsawBufferUsage::CL_ONLY:       os << "cl only";        break;
    case AFK_JigsawBufferUsage::CL_GL_COPIED:  os << "cl_gl copied";   break;
    case AFK_JigsawBufferUsage::CL_GL_SHARED:  os << "cl_gl shared";   break;
    default:
        throw AFK_Exception("Unrecognised buffer usage");
    }

    return os;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawDimensions dim)
{
    switch (dim)
    {
    case AFK_JigsawDimensions::TWO:    os << "2D";     break;
    case AFK_JigsawDimensions::THREE:  os << "3D";     break;
    default:
        throw AFK_Exception("Unrecognised jigsaw dimensions");
    }

    return os;
}


/* AFK_JigsawFormatDescriptor implementation */

AFK_JigsawFormatDescriptor::AFK_JigsawFormatDescriptor(AFK_JigsawFormat e)
{
    switch (e)
    {
    case AFK_JigsawFormat::UINT32:
        glInternalFormat                    = GL_R32UI;
        glFormat                            = GL_RED_INTEGER;
        glDataType                          = GL_UNSIGNED_INT;
        clFormat.image_channel_order        = CL_R;
        clFormat.image_channel_data_type    = CL_UNSIGNED_INT32;
        texelSize                           = sizeof(uint32_t);
        str                                 = "UINT32";
        break;

    case AFK_JigsawFormat::UINT32_2:
        glInternalFormat                    = GL_RG32UI;
        glFormat                            = GL_RG_INTEGER;
        glDataType                          = GL_UNSIGNED_INT;
        clFormat.image_channel_order        = CL_RG;
        clFormat.image_channel_data_type    = CL_UNSIGNED_INT32;
        texelSize                           = sizeof(unsigned int) * 2;
        str                                 = "UINT32_2";
        break;

    case AFK_JigsawFormat::FLOAT32:
        glInternalFormat                    = GL_R32F;
        glFormat                            = GL_RED;
        glDataType                          = GL_FLOAT;
        clFormat.image_channel_order        = CL_R;
        clFormat.image_channel_data_type    = CL_FLOAT;
        texelSize                           = sizeof(float);
        str                                 = "FLOAT32";
        break;

    case AFK_JigsawFormat::FLOAT8_UNORM_4:
        glInternalFormat                    = GL_RGBA8;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_UNSIGNED_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_UNORM_INT8;
        texelSize                           = sizeof(uint8_t) * 4;
        str                                 = "FLOAT8_UNORM_4";
        break;

    /* TODO cl_gl sharing seems to barf with this one ... */
    case AFK_JigsawFormat::FLOAT8_SNORM_4:
        glInternalFormat                    = GL_RGBA8_SNORM;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_SNORM_INT8;
        texelSize                           = sizeof(uint8_t) * 4;
        str                                 = "FLOAT8_SNORM_4";
        break;

    case AFK_JigsawFormat::FLOAT32_4:
        glInternalFormat                    = GL_RGBA32F;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_FLOAT;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_FLOAT;
        texelSize                           = sizeof(float) * 4;
        str                                 = "FLOAT32_4";
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
    str                                 = _fd.str;
}

AFK_JigsawFormatDescriptor AFK_JigsawFormatDescriptor::operator=(
    const AFK_JigsawFormatDescriptor& _fd)
{
    glInternalFormat                    = _fd.glInternalFormat;
    glFormat                            = _fd.glFormat;
    glDataType                          = _fd.glDataType;
    clFormat.image_channel_order        = _fd.clFormat.image_channel_order;
    clFormat.image_channel_data_type    = _fd.clFormat.image_channel_data_type;
    texelSize                           = _fd.texelSize;
    str                                 = _fd.str;
    return *this;
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawFormatDescriptor& _format)
{
    return os << _format.str;
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

std::ostream& operator<<(std::ostream& os, const AFK_JigsawFake3DDescriptor& _fake3D)
{
    if (_fake3D.getUseFake3D())
    {
        os << "fake 3D with 2D size " << _fake3D.get2DSize();
    }
    else
    {
        os << "real 3D";
    }

    return os;
}


/* AFK_JigsawImageDescriptor implementation */

AFK_JigsawImageDescriptor::AFK_JigsawImageDescriptor(
    const Vec3<int>& _pieceSize,
    AFK_JigsawFormat _format,
    AFK_JigsawDimensions _dimensions,
    AFK_JigsawBufferUsage _bufferUsage):
        pieceSize(_pieceSize),
        format(_format),
        dimensions(_dimensions),
        bufferUsage(_bufferUsage)
{
}

AFK_JigsawImageDescriptor::AFK_JigsawImageDescriptor(
    const AFK_JigsawImageDescriptor& _desc):
        pieceSize(_desc.pieceSize),
        format(_desc.format),
        dimensions(_desc.dimensions),
        bufferUsage(_desc.bufferUsage),
        fake3D(_desc.fake3D)
{
}

AFK_JigsawImageDescriptor AFK_JigsawImageDescriptor::operator=(
    const AFK_JigsawImageDescriptor& _desc)
{
    pieceSize       = _desc.pieceSize;
    format          = _desc.format;
    dimensions      = _desc.dimensions;
    bufferUsage     = _desc.bufferUsage;
    fake3D          = _desc.fake3D;
    return *this;
}

Vec3<int> AFK_JigsawImageDescriptor::getJigsawSize(
    int pieceCount,
    unsigned int concurrency,
    const AFK_ClDeviceProperties& _clDeviceProps) const
{
    /* Figure out a jigsaw size.  I want the rows to always be a
     * round multiple of `concurrency' to avoid breaking rectangles
     * apart.
     * For this I need to try all the formats: I stop testing
     * when any one of the formats fails, because all the
     * jigsaw textures need to be identical aside from their
     * texels
     */
    Vec3<int> jigsawSizeIncrement = (
        dimensions == AFK_JigsawDimensions::TWO ? afk_vec3<int>(concurrency, concurrency, 0) :
        afk_vec3<int>(concurrency, concurrency, concurrency));
    Vec3<int> jigsawSize = (
        dimensions == AFK_JigsawDimensions::TWO ? afk_vec3<int>(concurrency, concurrency, 1) :
        afk_vec3<int>(concurrency, concurrency, concurrency));

    GLuint proxyTexTarget = getGlProxyTarget();
    GLuint glProxyTex;
    glGenTextures(1, &glProxyTex);
    GLint texWidth;
    bool dimensionsOK = true;
    for (Vec3<int> testJigsawSize = jigsawSize;
        dimensionsOK && jigsawSize.v[0] * jigsawSize.v[1] < pieceCount;
        testJigsawSize += jigsawSizeIncrement)
    {
        dimensionsOK = (
            dimensions == AFK_JigsawDimensions::TWO ?
                (testJigsawSize.v[0] <= (int)_clDeviceProps.image2DMaxWidth &&
                 testJigsawSize.v[1] <= (int)_clDeviceProps.image2DMaxHeight) :
                (testJigsawSize.v[0] <= (int)_clDeviceProps.image3DMaxWidth &&
                 testJigsawSize.v[1] <= (int)_clDeviceProps.image3DMaxHeight &&
                 testJigsawSize.v[2] <= (int)_clDeviceProps.image3DMaxDepth));

        /* Try to make a pretend texture of the current jigsaw size */
        dimensionsOK &= ((testJigsawSize.v[0] * testJigsawSize.v[1] * testJigsawSize.v[2] * getPieceSizeInBytes()) < (_clDeviceProps.maxMemAllocSize / 2));
        if (!dimensionsOK) break;

        glBindTexture(proxyTexTarget, glProxyTex);
        switch (dimensions)
        {
        case AFK_JigsawDimensions::TWO:
            glTexImage2D(
                proxyTexTarget,
                0,
                format.glInternalFormat,
                pieceSize.v[0] * testJigsawSize.v[0],
                pieceSize.v[1] * testJigsawSize.v[1],
                0,
                format.glFormat,
                format.glDataType,
                nullptr);
            break;

        case AFK_JigsawDimensions::THREE:
            glTexImage3D(
                proxyTexTarget,
                0,
                format.glInternalFormat,
                pieceSize.v[0] * testJigsawSize.v[0],
                pieceSize.v[1] * testJigsawSize.v[1],
                pieceSize.v[2] * testJigsawSize.v[2],
				0,
                format.glFormat,
                format.glDataType,
                nullptr);
            break;

        default:
            throw AFK_Exception("Unrecognised proxyTexTarget");
        }

        /* See if it worked */
        glGetTexLevelParameteriv(
            proxyTexTarget,
            0,
            GL_TEXTURE_WIDTH,
            &texWidth);

        dimensionsOK &= (texWidth != 0);

        if (dimensionsOK) jigsawSize = testJigsawSize;

        /* Drop errors */
        glGetError();
    }

    glDeleteTextures(1, &glProxyTex);
    return jigsawSize;
}

void AFK_JigsawImageDescriptor::setUseFake3D(const Vec3<int>& _jigsawSize)
{
    assert(!fake3D.getUseFake3D()); /* no overwrites */
    assert(bufferUsage != AFK_JigsawBufferUsage::CL_GL_SHARED);
    fake3D = AFK_JigsawFake3DDescriptor(true, afk_vec3<int>(
        pieceSize.v[0] * _jigsawSize.v[0],
        pieceSize.v[1] * _jigsawSize.v[1],
        pieceSize.v[2] * _jigsawSize.v[2]));
}

cl_mem_object_type AFK_JigsawImageDescriptor::getClObjectType(void) const
{
    return (dimensions == AFK_JigsawDimensions::THREE && fake3D.getUseFake3D() ?
        CL_MEM_OBJECT_IMAGE3D : CL_MEM_OBJECT_IMAGE2D);
}

GLuint AFK_JigsawImageDescriptor::getGlTarget(void) const
{
    return (dimensions == AFK_JigsawDimensions::TWO ? GL_TEXTURE_2D : GL_TEXTURE_3D);
}

GLuint AFK_JigsawImageDescriptor::getGlProxyTarget(void) const
{
    return (dimensions == AFK_JigsawDimensions::TWO ? GL_PROXY_TEXTURE_2D : GL_PROXY_TEXTURE_3D);
}

size_t AFK_JigsawImageDescriptor::getPieceSizeInBytes(void) const
{
    return format.texelSize * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2];
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawImageDescriptor& _desc)
{
    os << "Image(" << _desc.pieceSize << ", " << _desc.format << ", " << _desc.dimensions << ", " << _desc.bufferUsage << ")";
    return os;
}


/* AFK_JigsawImage implementation */

void AFK_JigsawImage::getClChangeData(
    const std::vector<AFK_JigsawCuboid>& drawCuboids,
    cl_command_queue q,
    const std::vector<cl_event>& eventWaitList)
{
    size_t pieceSizeInBytes = desc.getPieceSizeInBytes();

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
  
        origin[0] = drawCuboids[cI].r * desc.pieceSize.v[0];
        origin[1] = drawCuboids[cI].c * desc.pieceSize.v[1];
        origin[2] = drawCuboids[cI].s * desc.pieceSize.v[2];
  
        region[0] = drawCuboids[cI].rows * desc.pieceSize.v[0];
        region[1] = drawCuboids[cI].columns.load() * desc.pieceSize.v[1];
        region[2] = drawCuboids[cI].slices * desc.pieceSize.v[2];

        changeData.resize(changeDataOffset + cuboidSizeInBytes);
        changeEvents.resize(changeEvent + 1);
  
        AFK_CLCHK(computer->oclShim.EnqueueReadImage()(
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
    size_t pieceSliceSizeInBytes = desc.getPieceSizeInBytes();
    assert(pieceSliceSizeInBytes > 0);

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
            for (int pieceSlice = 0; pieceSlice < desc.pieceSize.v[2]; ++pieceSlice)
            {
                size_t origin[3];
                size_t region[3];

                Vec2<int> clOrigin = desc.fake3D.fake3DTo2D(afk_vec3<int>(
                    drawCuboids[cI].r * desc.pieceSize.v[0],
                    drawCuboids[cI].c * desc.pieceSize.v[1],
                    (drawCuboids[cI].s + slice) * desc.pieceSize.v[2] + pieceSlice));
                origin[0] = clOrigin.v[0];
                origin[1] = clOrigin.v[1];
                origin[2] = 0;

                region[0] = drawCuboids[cI].rows * desc.pieceSize.v[0];
                region[1] = drawCuboids[cI].columns.load() * desc.pieceSize.v[1];
                region[2] = 1;

                changeData.resize(changeDataOffset + cuboidSliceSizeInBytes);
                changeEvents.resize(changeEvent + 1);

                AFK_CLCHK(computer->oclShim.EnqueueReadImage()(
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
    size_t pieceSizeInBytes = desc.getPieceSizeInBytes();
    size_t changeDataOffset = 0;
    for (unsigned int cI = 0; cI < drawCuboids.size(); ++cI)
    {
        size_t cuboidSizeInBytes = pieceSizeInBytes *
            drawCuboids[cI].rows *
            drawCuboids[cI].columns.load() *
            drawCuboids[cI].slices;

        if (cuboidSizeInBytes == 0) continue;

        switch (desc.dimensions)
        {
        case AFK_JigsawDimensions::TWO:
            glTexSubImage2D(
                desc.getGlTarget(), 0,
                drawCuboids[cI].r * desc.pieceSize.v[0],
                drawCuboids[cI].c * desc.pieceSize.v[1],
                drawCuboids[cI].rows * desc.pieceSize.v[0],
                drawCuboids[cI].columns.load() * desc.pieceSize.v[1],
                desc.format.glFormat,
                desc.format.glDataType,
                &changeData[changeDataOffset]);
            break;

        case AFK_JigsawDimensions::THREE:
            glTexSubImage3D(
                desc.getGlTarget(), 0,
                drawCuboids[cI].r * desc.pieceSize.v[0],
                drawCuboids[cI].c * desc.pieceSize.v[1],
                drawCuboids[cI].s * desc.pieceSize.v[2],
                drawCuboids[cI].rows * desc.pieceSize.v[0],
                drawCuboids[cI].columns.load() * desc.pieceSize.v[1],
                drawCuboids[cI].slices * desc.pieceSize.v[2],
                desc.format.glFormat,
                desc.format.glDataType,
                &changeData[changeDataOffset]);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }

        changeDataOffset += cuboidSizeInBytes;
    }
}

AFK_JigsawImage::AFK_JigsawImage(
    AFK_Computer *_computer,
    const Vec3<int>& _jigsawSize,
    const AFK_JigsawImageDescriptor& _desc):
        computer(_computer),
        glTex(0),
        clTex(0),
        desc(_desc)
{
    cl_int error;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    if (desc.bufferUsage != AFK_JigsawBufferUsage::CL_GL_SHARED)
    {
        /* Do native CL buffering here. */
        cl_image_desc imageDesc;
        memset(&imageDesc, 0, sizeof(cl_image_desc));

        cl_mem_object_type clImageType = desc.getClObjectType();

        Vec3<int> clImageSize;
        if (desc.fake3D.getUseFake3D())
        {
            clImageSize = desc.fake3D.get2DSize();
        }
        else
        {
            clImageSize = afk_vec3<int>(
                desc.pieceSize.v[0] * _jigsawSize.v[0],
                desc.pieceSize.v[1] * _jigsawSize.v[1],
                desc.pieceSize.v[2] * _jigsawSize.v[2]);
        }

        imageDesc.image_type        = clImageType;
        imageDesc.image_width       = clImageSize.v[0];
        imageDesc.image_height      = clImageSize.v[1];
        imageDesc.image_depth       = clImageSize.v[2];

        if (afk_core.computer->testVersion(1, 2))
        {
            clTex = computer->oclShim.CreateImage()(
                ctxt,
                CL_MEM_READ_WRITE,
                &desc.format.clFormat,
                &imageDesc,
                nullptr,
                &error);
        }
        else
        {
            switch (clImageType)
            {
            case CL_MEM_OBJECT_IMAGE2D:
                clTex = computer->oclShim.CreateImage2D()(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &desc.format.clFormat,
                    imageDesc.image_width,
                    imageDesc.image_height,
                    imageDesc.image_row_pitch,
                    nullptr,
                    &error);
                break;

            case CL_MEM_OBJECT_IMAGE3D:
                clTex = computer->oclShim.CreateImage3D()(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &desc.format.clFormat,
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
        }
    }

    if (desc.bufferUsage != AFK_JigsawBufferUsage::CL_ONLY)
    {
        GLuint texTarget = desc.getGlTarget();
        glGenTextures(1, &glTex);
        glBindTexture(texTarget, glTex);

        switch (desc.dimensions)
        {
        case AFK_JigsawDimensions::TWO:
            glTexStorage2D(
                texTarget,
                1,
                desc.format.glInternalFormat,
                desc.pieceSize.v[0] * _jigsawSize.v[0],
                desc.pieceSize.v[1] * _jigsawSize.v[1]);
            break;

        case AFK_JigsawDimensions::THREE:
            glTexStorage3D(
                texTarget,
                1,
                desc.format.glInternalFormat,
                desc.pieceSize.v[0] * _jigsawSize.v[0],
                desc.pieceSize.v[1] * _jigsawSize.v[1],
                desc.pieceSize.v[2] * _jigsawSize.v[2]);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }
        AFK_GLCHK("AFK_JigSaw texStorage")

        if (desc.bufferUsage == AFK_JigsawBufferUsage::CL_GL_SHARED)
        {
            if (afk_core.computer->testVersion(1, 2))
            {
                clTex = computer->oclShim.CreateFromGLTexture()(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    texTarget,
                    0,
                    glTex,
                    &error);
            }
            else
            {
                switch (desc.dimensions)
                {
                case AFK_JigsawDimensions::TWO:
                    clTex = computer->oclShim.CreateFromGLTexture2D()(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        texTarget,
                        0,
                        glTex,
                        &error);           
                    break;

                case AFK_JigsawDimensions::THREE:
                    clTex = computer->oclShim.CreateFromGLTexture3D()(
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
            }
            AFK_HANDLE_CL_ERROR(error);
        }
    }

    computer->unlock();
}

AFK_JigsawImage::~AFK_JigsawImage()
{
    if (clTex) computer->oclShim.ReleaseMemObject()(clTex);
    for (auto ev : changeEvents)
        if (ev) computer->oclShim.ReleaseEvent()(ev);
    if (glTex) glDeleteTextures(1, &glTex);
}

Vec2<int> AFK_JigsawImage::getFake3D_size(void) const
{
    Vec2<int> size2;

    if (desc.fake3D.getUseFake3D())
    {
        Vec3<int> size = desc.fake3D.getFakeSize();
        size2 = afk_vec2<int>(size.v[0], size.v[1]);
    }
    else
    {
        size2 = afk_vec2<int>(-1, -1);
    }

    return size2;
}

int AFK_JigsawImage::getFake3D_mult(void) const
{
    return desc.fake3D.getUseFake3D() ? desc.fake3D.getMult() : 0;
}

cl_mem AFK_JigsawImage::acquireForCl(std::vector<cl_event>& o_events)
{
    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    switch (desc.bufferUsage)
    {
    case AFK_JigsawBufferUsage::CL_GL_SHARED:
        cl_event acquireEvent;
        AFK_CLCHK(computer->oclShim.EnqueueAcquireGLObjects()(q, 1, &clTex, changeEvents.size(), &changeEvents[0], &acquireEvent))
        for (auto ev : changeEvents)
        {
            AFK_CLCHK(computer->oclShim.ReleaseEvent()(ev))
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
            AFK_CLCHK(computer->oclShim.RetainEvent()(ev))
            o_events.push_back(ev);
        }
        break;
    }

    computer->unlock();
    return clTex;
}

void AFK_JigsawImage::releaseFromCl(const std::vector<AFK_JigsawCuboid>& drawCuboids, const std::vector<cl_event>& eventWaitList)
{
    std::vector<cl_event> allWaitList;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    switch (desc.bufferUsage)
    {
    case AFK_JigsawBufferUsage::CL_GL_COPIED:
        /* Make sure that anything pending with that change data has been
         * finished up because I'm about to re-use the buffer.
         */
        if (changeEvents.size() > 0)
        {
            AFK_CLCHK(computer->oclShim.WaitForEvents()(changeEvents.size(), &changeEvents[0]))
            for (auto ev : changeEvents)
            {
                AFK_CLCHK(computer->oclShim.ReleaseEvent()(ev))
            }
        }

        /* Now, read back all the pieces, appending the data to `changeData'
         * in order.
         * So long as `bindTexture' writes them in the same order everything
         * will be okay!
         */
        if (desc.fake3D.getUseFake3D())
            getClChangeDataFake3D(drawCuboids, q, eventWaitList);
        else
            getClChangeData(drawCuboids, q, eventWaitList);
        break;

    case AFK_JigsawBufferUsage::CL_GL_SHARED:
        /* A release here is contingent on any old change events
         * being finished, of course.
         */
        allWaitList.reserve(eventWaitList.size() + changeEvents.size());
        for (auto ev : eventWaitList)
        {
            AFK_CLCHK(computer->oclShim.RetainEvent()(ev))
            allWaitList.push_back(ev);
        }
        std::copy(changeEvents.begin(), changeEvents.end(), allWaitList.end());

        changeEvents.resize(1);
        AFK_CLCHK(computer->oclShim.EnqueueReleaseGLObjects()(q, 1, &clTex, allWaitList.size(), &allWaitList[0], &changeEvents[0]))
        for (auto aw : allWaitList)
        {
            AFK_CLCHK(computer->oclShim.ReleaseEvent()(aw))
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
            AFK_CLCHK(computer->oclShim.RetainEvent()(ev))
            changeEvents.push_back(ev);
        }
        break;
    }

    computer->unlock();
}

void AFK_JigsawImage::bindTexture(const std::vector<AFK_JigsawCuboid>& drawCuboids)
{
    glBindTexture(desc.getGlTarget(), glTex);

    if (desc.bufferUsage == AFK_JigsawBufferUsage::CL_GL_COPIED)
    {
        /* Wait for the change readback to be finished. */
        if (changeEvents.size() == 0) return;

        AFK_CLCHK(computer->oclShim.WaitForEvents()(changeEvents.size(), &changeEvents[0]))
        for (unsigned int e = 0; e < changeEvents.size(); ++e)
        {
            AFK_CLCHK(computer->oclShim.ReleaseEvent()(changeEvents[e]))
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
        AFK_CLCHK(computer->oclShim.WaitForEvents()(changeEvents.size(), &changeEvents[0]))
        for (auto ev : changeEvents)
        {
            AFK_CLCHK(computer->oclShim.ReleaseEvent()(ev))
        }
        changeEvents.clear();
    }
}

