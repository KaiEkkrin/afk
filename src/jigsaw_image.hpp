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

#ifndef _AFK_JIGSAW_IMAGE_H_
#define _AFK_JIGSAW_IMAGE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include "def.hpp"
#include "computer.hpp"
#include "jigsaw_cuboid.hpp"

/* This module defines the container for a single image within a jigsaw.
 * For longer commentary, see Jigsaw.
 */

/* This enumeration describes the jigsaw's texture format.  I'll
 * need to add more here as I support more formats.
 */
enum class AFK_JigsawFormat : int
{
    UINT32          = 0,
    UINT32_2        = 1,
    FLOAT32         = 2,
    FLOAT8_UNORM_4  = 3,
    FLOAT8_SNORM_4  = 4,
    FLOAT32_4       = 5
};

/* This enumeration describes what buffers we manage -- CL, GL, both. */
enum class AFK_JigsawBufferUsage : int
{
    CL_ONLY         = 0,
    CL_GL_COPIED    = 1,
    CL_GL_SHARED    = 2
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawBufferUsage bufferUsage);

enum class AFK_JigsawDimensions : int
{
    TWO     = 0,
    THREE   = 1 
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawDimensions dim);

class AFK_JigsawFormatDescriptor
{
public:
    GLint glInternalFormat;
    GLenum glFormat;
    GLenum glDataType;
    cl_image_format clFormat;
    size_t texelSize;
    std::string str;

    AFK_JigsawFormatDescriptor(AFK_JigsawFormat);

    AFK_JigsawFormatDescriptor(const AFK_JigsawFormatDescriptor& _fd);
    AFK_JigsawFormatDescriptor operator=(const AFK_JigsawFormatDescriptor& _fd);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawFormatDescriptor& _format);

enum class AFK_Fake3D : int
{
    REAL_3D = 0,
    FAKE_3D = 1
};

/* This class describes a fake 3D image that emulates 3D with a
 * 2D image.
 * A fake 3D texture will be 2D for CL operations, and 3D for
 * GL operations.  The Jigsaw will hold its 3D dimensions, and
 * query an object of this class to obtain the CL dimensions.
 */
class AFK_JigsawFake3DDescriptor
{
    /* This is the emulated 3D piece size */
    Vec3<int> fakeSize;

    /* This is the multiplier used to achieve that fakery */
    int mult;

    /* This flags whether to use fake 3D in the first place */
    bool useFake3D;
public:

    /* This one initialises it to false. */
    AFK_JigsawFake3DDescriptor();

    AFK_JigsawFake3DDescriptor(bool _useFake3D, const Vec3<int>& _fakeSize);
    AFK_JigsawFake3DDescriptor(const AFK_JigsawFake3DDescriptor& _fake3D);
    AFK_JigsawFake3DDescriptor operator=(const AFK_JigsawFake3DDescriptor& _fake3D);

    bool getUseFake3D(void) const;
    Vec3<int> get2DSize(void) const;
    Vec3<int> getFakeSize(void) const;
    int getMult(void) const;

    /* Convert to and from the real 2D / emulated 3D. */
    Vec2<int> fake3DTo2D(const Vec3<int>& _fake) const;
    Vec3<int> fake3DFrom2D(const Vec2<int>& _real) const;
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawFake3DDescriptor& _fake3D);

class AFK_JigsawImageDescriptor
{
public:
    Vec3<int> pieceSize;
    AFK_JigsawFormatDescriptor format;
    AFK_JigsawDimensions dimensions;
    AFK_JigsawBufferUsage bufferUsage;
    AFK_JigsawFake3DDescriptor fake3D;

    /* TODO: I wanted to use an initializer list here, but I get
     * compile errors all over the shop :(
     */
    AFK_JigsawImageDescriptor(
        const Vec3<int>& _pieceSize,
        AFK_JigsawFormat _format,
        AFK_JigsawDimensions _dimensions,
        AFK_JigsawBufferUsage _bufferUsage);

    AFK_JigsawImageDescriptor(const AFK_JigsawImageDescriptor& _desc);
    AFK_JigsawImageDescriptor operator=(const AFK_JigsawImageDescriptor& _desc);

    /* Finds the biggest jigsaw dimensions that the GL will
     * accept for this image.
     * For 2D images, the slices value will be 1.
     */
    Vec3<int> getJigsawSize(
        int pieceCount,
        unsigned int concurrency,
        const AFK_ClDeviceProperties& _clDeviceProps) const;

    /* Enables fake 3D. */
    void setUseFake3D(const Vec3<int>& _jigsawSize);

    cl_mem_object_type getClObjectType(void) const;
    GLuint getGlTarget(void) const;
    GLuint getGlProxyTarget(void) const;
    size_t getPieceSizeInBytes(void) const;
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawImageDescriptor& _desc);

/* Note that AFK_JigsawImage is _not synchronized_: the synchronization is
 * done in the Jigsaw.
 */
class AFK_JigsawImage
{
protected:
    AFK_Computer *computer;

    /* The backing memory in GL and CL. */
    GLuint glTex;
    cl_mem clTex;

    /* Descriptions of the format of this image. */
    const AFK_JigsawImageDescriptor desc;

    /* If bufferUsage is cl gl copied, this is the cuboid data I've
     * read back from the CL and that needs to go into the GL.
     */
    std::vector<uint8_t> changeData;

    /* If bufferUsage is cl gl copied, these are the events I need to
     * wait on before I can push data to the GL.
     */
    std::vector<cl_event> changeEvents;

    /* These functions help to pull changed data from the CL
     * textures and push them to the GL.
     */
    void getClChangeData(
        const std::vector<AFK_JigsawCuboid>& drawCuboids,
        cl_command_queue q,
        const std::vector<cl_event>& eventWaitList);
    void getClChangeDataFake3D(
        const std::vector<AFK_JigsawCuboid>& drawCuboids,
        cl_command_queue q,
        const std::vector<cl_event>& eventWaitList);
    void putClChangeData(const std::vector<AFK_JigsawCuboid>& drawCuboids);

public:
    AFK_JigsawImage(
        AFK_Computer *_computer,
        const Vec3<int>& _jigsawSize,
        const AFK_JigsawImageDescriptor& _desc);
    virtual ~AFK_JigsawImage();

    Vec2<int> getFake3D_size(void) const;
    int getFake3D_mult(void) const;

    /* TODO: For the next two functions -- I'm now going around re-
     * acquiring jigsaws for read after having first acquired them
     * for write, and all sorts.  In which case I don't need to re-blit
     * all the same data.  Consider optimising so that data is only
     * copied over to the GL after a write acquire is released?
     */

    /* Acquires this image for the CL. */
    cl_mem acquireForCl(std::vector<cl_event>& o_events);

    /* Releases this image from the CL. */
    void releaseFromCl(const std::vector<AFK_JigsawCuboid>& drawCuboids, const std::vector<cl_event>& eventWaitList);

    /* Binds this image to the GL as a texture. */
    void bindTexture(const std::vector<AFK_JigsawCuboid>& drawCuboids);

    /* Makes sure all events are finished. */
    void waitForAll(void);
};

#endif /* _AFK_JIGSAW_IMAGE_H_ */
