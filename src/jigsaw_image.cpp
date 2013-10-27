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
#include <cstdlib>
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
    case AFK_JigsawBufferUsage::NO_IMAGE:       os << "no image";       break;
    case AFK_JigsawBufferUsage::CL_ONLY:        os << "cl only";        break;
    case AFK_JigsawBufferUsage::GL_ONLY:        os << "gl only";        break;
    case AFK_JigsawBufferUsage::CL_GL_COPIED:   os << "cl_gl copied";   break;
    case AFK_JigsawBufferUsage::CL_GL_SHARED:   os << "cl_gl shared";   break;
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

AFK_JigsawFormatDescriptor& AFK_JigsawFormatDescriptor::operator=(
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

AFK_JigsawFake3DDescriptor& AFK_JigsawFake3DDescriptor::operator=(
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

AFK_JigsawImageDescriptor& AFK_JigsawImageDescriptor::operator=(
    const AFK_JigsawImageDescriptor& _desc)
{
    pieceSize       = _desc.pieceSize;
    format          = _desc.format;
    dimensions      = _desc.dimensions;
    bufferUsage     = _desc.bufferUsage;
    fake3D          = _desc.fake3D;
    return *this;
}

bool AFK_JigsawImageDescriptor::isJigsawSizeOK(
    const Vec3<int>& jigsawSize,
    const AFK_ClDeviceProperties& _clDeviceProps) const
{
    bool dimensionsOK = (
        dimensions == AFK_JigsawDimensions::TWO ?
            (jigsawSize.v[0] <= (int)_clDeviceProps.image2DMaxWidth &&
             jigsawSize.v[1] <= (int)_clDeviceProps.image2DMaxHeight) :
            (jigsawSize.v[0] <= (int)_clDeviceProps.image3DMaxWidth &&
             jigsawSize.v[1] <= (int)_clDeviceProps.image3DMaxHeight &&
             jigsawSize.v[2] <= (int)_clDeviceProps.image3DMaxDepth));

    dimensionsOK &= ((jigsawSize.v[0] * jigsawSize.v[1] * jigsawSize.v[2] * getPieceSizeInBytes()) < _clDeviceProps.maxMemAllocSize);
    if (!dimensionsOK) return false;

    /* Try to make a pretend texture of this jigsaw size
     */
    GLuint proxyTexTarget = getGlProxyTarget();
    GLuint glProxyTex;
    glGenTextures(1, &glProxyTex);
    glBindTexture(proxyTexTarget, glProxyTex);
    switch (dimensions)
    {
    case AFK_JigsawDimensions::TWO:
        glTexImage2D(
            proxyTexTarget,
            0,
            format.glInternalFormat,
            pieceSize.v[0] * jigsawSize.v[0],
            pieceSize.v[1] * jigsawSize.v[1],
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
            pieceSize.v[0] * jigsawSize.v[0],
            pieceSize.v[1] * jigsawSize.v[1],
            pieceSize.v[2] * jigsawSize.v[2],
			0,
            format.glFormat,
            format.glDataType,
            nullptr);
        break;

    default:
        throw AFK_Exception("Unrecognised proxyTexTarget");
    }

    /* See if it worked */
    GLint texWidth = 0;
    glGetTexLevelParameteriv(
        proxyTexTarget,
        0,
        GL_TEXTURE_WIDTH,
        &texWidth);

    glDeleteTextures(1, &glProxyTex);

    /* Drop errors */
    glGetError();

    return (texWidth != 0);
}

void AFK_JigsawImageDescriptor::setUseFake3D(const Vec3<int>& _jigsawSize)
{
    assert(!fake3D.getUseFake3D()); /* no overwrites */
    assert(bufferUsage != AFK_JigsawBufferUsage::CL_GL_SHARED &&
        bufferUsage != AFK_JigsawBufferUsage::GL_ONLY);
    fake3D = AFK_JigsawFake3DDescriptor(true, afk_vec3<int>(
        pieceSize.v[0] * _jigsawSize.v[0],
        pieceSize.v[1] * _jigsawSize.v[1],
        pieceSize.v[2] * _jigsawSize.v[2]));
}

cl_mem_object_type AFK_JigsawImageDescriptor::getClObjectType(void) const
{
    return (dimensions == AFK_JigsawDimensions::THREE && !fake3D.getUseFake3D() ?
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

size_t AFK_JigsawImageDescriptor::getImageSizeInBytes(const Vec3<int>& _jigsawSize) const
{
    return format.texelSize *
        pieceSize.v[0] * _jigsawSize.v[0] *
        pieceSize.v[1] * _jigsawSize.v[1] *
        pieceSize.v[2] * _jigsawSize.v[2];
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawImageDescriptor& _desc)
{
    os << "Image(" << _desc.pieceSize << ", " << _desc.format << ", " << _desc.dimensions << ", " << _desc.bufferUsage << ", " << _desc.fake3D << ")";
    return os;
}


/* AFK_JigsawMemoryAllocation implementation */

AFK_JigsawMemoryAllocation::Entry::Entry(
    std::initializer_list<AFK_JigsawImageDescriptor> _desc,
    unsigned int _puzzleCount,
    float _ratio):
        desc(_desc), puzzleCount(_puzzleCount), ratio(_ratio),
        jigsawSizeSet(false)
{
    auto dIt = desc.begin();
    dimensions = dIt->dimensions;

    while ((++dIt) != desc.end())
        assert(dIt->dimensions == dimensions);
}

size_t AFK_JigsawMemoryAllocation::Entry::getTotalPieceSizeInBytes(void) const
{
    size_t totalSize = 0;
    for (auto d : desc)
        totalSize += d.getPieceSizeInBytes();       
    return totalSize;
}

float AFK_JigsawMemoryAllocation::Entry::getBytewiseRatio(void) const
{
    return ratio * (float)puzzleCount * (float)getTotalPieceSizeInBytes();
}

void AFK_JigsawMemoryAllocation::Entry::makeJigsawSize(
    unsigned int concurrency,
    size_t maxSizeInBytes,
    const AFK_ClDeviceProperties& _clDeviceProps)
{
    /* Figure out a jigsaw size.  I want the rows to always be a
     * round multiple of `concurrency' to avoid breaking rectangles
     * apart.
     */
    Vec3<int> jigsawSizeIncrement = (
        dimensions == AFK_JigsawDimensions::TWO ? afk_vec3<int>(concurrency, concurrency, 0) :
        afk_vec3<int>(concurrency, concurrency, concurrency));
    jigsawSize = (
        dimensions == AFK_JigsawDimensions::TWO ? afk_vec3<int>(concurrency, concurrency, 1) :
        afk_vec3<int>(concurrency, concurrency, concurrency));

    bool dimensionsOK = true;
    for (Vec3<int> testJigsawSize = jigsawSize;
        dimensionsOK;
        testJigsawSize += jigsawSizeIncrement)
    {
        size_t sizeOfAllImages = 0;

        for (auto d : desc)
        {
            dimensionsOK &= d.isJigsawSizeOK(testJigsawSize, _clDeviceProps);
            sizeOfAllImages += d.getImageSizeInBytes(testJigsawSize);
        }

        dimensionsOK &= (maxSizeInBytes == 0 || sizeOfAllImages < maxSizeInBytes);

        if (dimensionsOK) jigsawSize = testJigsawSize;
    }

    jigsawSizeSet = true;
}

void AFK_JigsawMemoryAllocation::Entry::setUseFake3D(void)
{
    for (auto dIt = desc.begin(); dIt != desc.end(); ++dIt)
    {
        if (dIt->dimensions == AFK_JigsawDimensions::THREE) dIt->setUseFake3D(jigsawSize);
    }
}

unsigned int AFK_JigsawMemoryAllocation::Entry::getPieceCount(void) const
{
    assert(jigsawSizeSet);
    unsigned int pieceCount = jigsawSize.v[0] * jigsawSize.v[1] * jigsawSize.v[2];
    assert(pieceCount > 0);
    return pieceCount;
}

unsigned int AFK_JigsawMemoryAllocation::Entry::getPuzzleCount(void) const
{
    return puzzleCount;
}

Vec3<int> AFK_JigsawMemoryAllocation::Entry::getJigsawSize(void) const
{
    assert(jigsawSizeSet);
    return jigsawSize;
}

std::vector<AFK_JigsawImageDescriptor>::const_iterator AFK_JigsawMemoryAllocation::Entry::beginDescriptors() const
{
    return desc.begin();
}

std::vector<AFK_JigsawImageDescriptor>::const_iterator AFK_JigsawMemoryAllocation::Entry::endDescriptors() const
{
    return desc.end();
}

AFK_JigsawMemoryAllocation::AFK_JigsawMemoryAllocation(
    std::initializer_list<AFK_JigsawMemoryAllocation::Entry> _entries,
    unsigned int concurrency,
    bool useFake3D,
    float proportionOfMaxSizeToUse,
    const AFK_ClDeviceProperties& _clDeviceProps):
        entries(_entries)
{
    /* Work out the `real' ratios.  I need to multiply up by
     * the puzzle counts, and also by the byte piece sizes,
     * since what I've been given is a piece count ratio
     */
    float ratioTotal = 0.0f;
    for (auto e : entries)
        ratioTotal += e.getBytewiseRatio();

    /* And now that I've got those figures I can cram them into
     * the space that the CL reports is available (which will be
     * way less than all of it, so I can assume that there will
     * be room for mirror buffers in the GL as required, I think)
     */
    for (auto eIt = entries.begin(); eIt != entries.end(); ++eIt)
    {
        eIt->makeJigsawSize(
            concurrency,
            (size_t)(
                (float)_clDeviceProps.maxMemAllocSize * proportionOfMaxSizeToUse * eIt->getBytewiseRatio() / ratioTotal),
            _clDeviceProps);
        if (useFake3D) eIt->setUseFake3D();
    }
}

const AFK_JigsawMemoryAllocation::Entry& AFK_JigsawMemoryAllocation::at(unsigned int entry) const
{
    return entries.at(entry);
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawMemoryAllocation::Entry& _entry)
{
    if (_entry.jigsawSizeSet)
    {
        os << "Jigsaw memory allocation entry with size " << _entry.jigsawSize << ", using " <<
            (float)(_entry.getTotalPieceSizeInBytes() * _entry.getPieceCount()) / (1024.0f * 1024.0f) << " MB :" << std::endl;
    }
    else
    {
        os << "Uncalculated jigsaw memory allocation" << std::endl;
    }

    for (auto d : _entry.desc) os << d << std::endl;
    return os;
}


/* AFK_JigsawImage implementation */

void AFK_JigsawImage::initClImage(const Vec3<int>& _jigsawSize)
{
    cl_int error;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

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

#ifdef CL_VERSION_1_2
    if (computer->testVersion(1, 2))
    {
        cl_image_desc imageDesc;
        memset(&imageDesc, 0, sizeof(cl_image_desc));

        imageDesc.image_type        = clImageType;
        imageDesc.image_width       = clImageSize.v[0];
        imageDesc.image_height      = clImageSize.v[1];
        imageDesc.image_depth       = clImageSize.v[2];

        clTex = computer->oclShim.CreateImage()(
            ctxt,
            CL_MEM_READ_WRITE,
            &desc.format.clFormat,
            &imageDesc,
            nullptr,
            &error);
    }
    else
#endif
    {
        switch (clImageType)
        {
        case CL_MEM_OBJECT_IMAGE2D:
            clTex = computer->oclShim.CreateImage2D()(
                ctxt,
                CL_MEM_READ_WRITE,
                &desc.format.clFormat,
                clImageSize.v[0],
                clImageSize.v[1],
                0,
                nullptr,
                &error);
            break;

        case CL_MEM_OBJECT_IMAGE3D:
            clTex = computer->oclShim.CreateImage3D()(
                ctxt,
                CL_MEM_READ_WRITE,
                &desc.format.clFormat,
                clImageSize.v[0],
                clImageSize.v[1],
                clImageSize.v[2],
                0,
                0,
                nullptr,
                &error);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }
    }

    AFK_HANDLE_CL_ERROR(error);
    computer->unlock();
}

void AFK_JigsawImage::initGlImage(const Vec3<int>& _jigsawSize)
{
    GLuint texTarget = desc.getGlTarget();
    glGenTextures(1, &glTex);
    glBindTexture(texTarget, glTex);

    /* Switching away from using glTexStorage2D() here because
     * it requires a more recent GLEW, which is inconvenient in
     * Ubuntu 12.04 LTS.
     * initGlImage() is not called very often, so it's okay
     * to be pushing zeroes here.
     */
    size_t zeroBufSize = desc.getImageSizeInBytes(_jigsawSize);
    assert(zeroBufSize > 0);
    void *zeroBuf = calloc(zeroBufSize, 1);

    switch (desc.dimensions)
    {
    case AFK_JigsawDimensions::TWO:
        glTexImage2D(
            texTarget,
            0,
            desc.format.glInternalFormat,
            desc.pieceSize.v[0] * _jigsawSize.v[0],
            desc.pieceSize.v[1] * _jigsawSize.v[1],
            0,
            desc.format.glFormat,
            desc.format.glDataType,
            zeroBuf);
        break;

    case AFK_JigsawDimensions::THREE:
        glTexImage3D(
            texTarget,
            0,
            desc.format.glInternalFormat,
            desc.pieceSize.v[0] * _jigsawSize.v[0],
            desc.pieceSize.v[1] * _jigsawSize.v[1],
            desc.pieceSize.v[2] * _jigsawSize.v[2],
            0,
            desc.format.glFormat,
            desc.format.glDataType,
            zeroBuf);
        break;

    default:
        throw AFK_Exception("Unrecognised texTarget");
    }

    AFK_GLCHK("AFK_JigSaw texStorage")
    free(zeroBuf);
}

void AFK_JigsawImage::initClImageFromGlImage(const Vec3<int>& _jigsawSize)
{
    cl_int error;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    GLuint texTarget = desc.getGlTarget();
    glBindTexture(texTarget, glTex);

#ifdef CL_VERSION_1_2
    if (computer->testVersion(1, 2))
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
#endif
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
    computer->unlock();
}

void AFK_JigsawImage::resizeChangeData(const std::vector<AFK_JigsawCuboid>& drawCuboids)
{
    size_t pieceSizeInBytes = desc.getPieceSizeInBytes();
    size_t changeDataSize = 0;
    for (unsigned int cI = 0; cI < drawCuboids.size(); ++cI)
    {
        changeDataSize += (pieceSizeInBytes *
            drawCuboids[cI].rows *
            drawCuboids[cI].columns.load() *
            drawCuboids[cI].slices);
    }

    changeData.resize(changeDataSize);
}

void AFK_JigsawImage::getClChangeData(
    const std::vector<AFK_JigsawCuboid>& drawCuboids,
    cl_command_queue q,
    const AFK_ComputeDependency& dep)
{
    resizeChangeData(drawCuboids);

    size_t pieceSizeInBytes = desc.getPieceSizeInBytes();
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
  
        AFK_CLCHK(computer->oclShim.EnqueueReadImage()(
            q, clTex, CL_FALSE, origin, region, 0, 0, &changeData[changeDataOffset],
                dep.getEventCount(), dep.getEvents(), changeDep.addEvent()))
        changeDataOffset += cuboidSizeInBytes;
    }

    assert(changeDataOffset == changeData.size());
}

void AFK_JigsawImage::getClChangeDataFake3D(
    const std::vector<AFK_JigsawCuboid>& drawCuboids,
    cl_command_queue q,
    const AFK_ComputeDependency& dep)
{
    /* I should be moving exactly the same amount of data as
     * if it were a real 3D image.
     */
    resizeChangeData(drawCuboids);

    size_t pieceSliceSizeInBytes = desc.format.texelSize * desc.pieceSize.v[0] * desc.pieceSize.v[1];
    assert(pieceSliceSizeInBytes > 0);

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

                AFK_CLCHK(computer->oclShim.EnqueueReadImage()(
                    q, clTex, CL_FALSE, origin, region, 0, 0, &changeData[changeDataOffset],
                        dep.getEventCount(), dep.getEvents(), changeDep.addEvent()))
                changeDataOffset += cuboidSliceSizeInBytes;
            }
        }
    }

    assert(changeDataOffset == changeData.size());
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

    assert(changeDataOffset == changeData.size());
    changeData.clear();
}

AFK_JigsawImage::AFK_JigsawImage(
    AFK_Computer *_computer,
    const Vec3<int>& _jigsawSize,
    const AFK_JigsawImageDescriptor& _desc):
        computer(_computer),
        glTex(0),
        clTex(0),
        desc(_desc),
        changeDep(_computer)
{
    switch (desc.bufferUsage)
    {
    case AFK_JigsawBufferUsage::NO_IMAGE:
        /* Nothing to do :P */
        break;

    case AFK_JigsawBufferUsage::CL_ONLY:
        initClImage(_jigsawSize);
        break;

    case AFK_JigsawBufferUsage::GL_ONLY:
        initGlImage(_jigsawSize);
        break;

    case AFK_JigsawBufferUsage::CL_GL_COPIED:
        initClImage(_jigsawSize);
        initGlImage(_jigsawSize);
        break;

    case AFK_JigsawBufferUsage::CL_GL_SHARED:
        initGlImage(_jigsawSize);
        initClImageFromGlImage(_jigsawSize);
        break;
    }
}

AFK_JigsawImage::~AFK_JigsawImage()
{
    if (clTex) computer->oclShim.ReleaseMemObject()(clTex);
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

cl_mem AFK_JigsawImage::acquireForCl(AFK_ComputeDependency& o_dep)
{
    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    switch (desc.bufferUsage)
    {
    case AFK_JigsawBufferUsage::NO_IMAGE:
    case AFK_JigsawBufferUsage::GL_ONLY:
        throw AFK_Exception("Called acquireForCl() on jigsaw image without CL");

    case AFK_JigsawBufferUsage::CL_GL_SHARED:
        AFK_CLCHK(computer->oclShim.EnqueueAcquireGLObjects()(q, 1, &clTex, changeDep.getEventCount(), changeDep.getEvents(), o_dep.addEvent()))
        break;

    default:
        /* If there are leftover events the caller needs to wait
         * for them ...
         */
        o_dep += changeDep;
        break;
    }

    computer->unlock();
    return clTex;
}

void AFK_JigsawImage::releaseFromCl(const std::vector<AFK_JigsawCuboid>& drawCuboids, const AFK_ComputeDependency& dep)
{
    std::vector<cl_event> allWaitList;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);

    switch (desc.bufferUsage)
    {
    case AFK_JigsawBufferUsage::NO_IMAGE:
    case AFK_JigsawBufferUsage::GL_ONLY:
        throw AFK_Exception("Called releaseFrom() on jigsaw image without CL");

    case AFK_JigsawBufferUsage::CL_ONLY:
        /* The supplied events get retained and fed into the
         * change event list, so that they can be waited for
         * upon any subsequent acquire.
         * This replaces any prior events (which the caller
         * had to wait for, directly or indirectly, by calling
         * acquireForCl() earlier.)
         */
        changeDep = dep;
        break;

    case AFK_JigsawBufferUsage::CL_GL_COPIED:
        /* Make sure that anything pending with that change data has been
         * finished up because I'm about to re-use the buffer.
         */
        changeDep.waitFor();

        /* Now, read back all the pieces, appending the data to `changeData'
         * in order.
         * So long as `bindTexture' writes them in the same order everything
         * will be okay!
         */
        if (desc.fake3D.getUseFake3D())
            getClChangeDataFake3D(drawCuboids, q, dep);
        else
            getClChangeData(drawCuboids, q, dep);
        break;

    case AFK_JigsawBufferUsage::CL_GL_SHARED:
        /* Make sure that anything pending with that change data has been
         * finished up because I'm about to re-use the buffer.
         */
        changeDep.waitFor();

        AFK_CLCHK(computer->oclShim.EnqueueReleaseGLObjects()(q, 1, &clTex, dep.getEventCount(), dep.getEvents(), changeDep.addEvent()))
        break;
    }

    computer->unlock();
}

void AFK_JigsawImage::bindTexture(const std::vector<AFK_JigsawCuboid>& drawCuboids)
{

    switch (desc.bufferUsage)
    {
    case AFK_JigsawBufferUsage::NO_IMAGE:
    case AFK_JigsawBufferUsage::CL_ONLY:
        throw AFK_Exception("Called bindTexture() on jigsaw image without GL");

    case AFK_JigsawBufferUsage::GL_ONLY:
        /* I shouldn't have any change events to worry about. */
        assert(changeDep.getEventCount() == 0);
        glBindTexture(desc.getGlTarget(), glTex);
        break;

    case AFK_JigsawBufferUsage::CL_GL_COPIED:
    case AFK_JigsawBufferUsage::CL_GL_SHARED:
        /* Wait for any sync-up to be finished. */
        changeDep.waitFor();

        glBindTexture(desc.getGlTarget(), glTex);
        if (desc.bufferUsage == AFK_JigsawBufferUsage::CL_GL_COPIED)
            putClChangeData(drawCuboids);
        break;
    }
}

void AFK_JigsawImage::waitForAll(void)
{
    changeDep.waitFor();
}

