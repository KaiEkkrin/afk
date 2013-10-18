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

#include <vector>

#include "def.hpp"
#include "jigsaw_cuboid.hpp"

/* This module defines the container for a single image within a jigsaw.
 * For longer commentary, see Jigsaw.
 */

/* This enumeration describes the jigsaw's texture format.  I'll
 * need to add more here as I support more formats.
 */
enum AFK_JigsawFormat
{
    AFK_JIGSAW_UINT32,
    AFK_JIGSAW_2UINT32,
    AFK_JIGSAW_FLOAT32,
    AFK_JIGSAW_555A1,
    AFK_JIGSAW_101010A2,
    AFK_JIGSAW_4FLOAT8_UNORM,
    AFK_JIGSAW_4FLOAT8_SNORM,
    AFK_JIGSAW_4FLOAT32
};

/* This enumeration describes what buffers we manage -- CL, GL, both. */
enum AFK_JigsawBufferUsage
{
    AFK_JIGSAW_BU_CL_ONLY,
    AFK_JIGSAW_BU_CL_GL_COPIED,
    AFK_JIGSAW_BU_CL_GL_SHARED
};

class AFK_JigsawFormatDescriptor
{
public:
    GLint glInternalFormat;
    GLenum glFormat;
    GLenum glDataType;
    cl_image_format clFormat;
    size_t texelSize;

    AFK_JigsawFormatDescriptor(enum AFK_JigsawFormat);
    AFK_JigsawFormatDescriptor(const AFK_JigsawFormatDescriptor& _fd);
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

/* Note that AFK_JigsawImage is _not synchronized_: the synchronization is
 * done in the Jigsaw.
 */
class AFK_JigsawImage
{
protected:
    /* The backing memory in GL and CL. */
    GLuint glTex;
    cl_mem clTex;

    /* Descriptions of the format of this image. */
    const AFK_JigsawFormatDescriptor format;
    const GLuint texTarget;
    const AFK_JigsawFake3DDescriptor fake3D;
    const cl_mem_object_type clImageType;

    /* How big the pieces are.
     * TODO: I want to eventually vary this; but for now, keep
     * them the same to make sure everything still works ...
     */
    const Vec3<int> pieceSize;

    /* How to handle the buffers (copy, share, etc.) */
    const enum AFK_JigsawBufferUsage bufferUsage;

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
        cl_context ctxt,
        const Vec3<int>& _pieceSize,
        const Vec3<int>& _jigsawSize,
        const AFK_JigsawFormatDescriptor& _format,
        GLuint _texTarget,
        const AFK_JigsawFake3DDescriptor& _fake3D,
        enum AFK_JigsawBufferUsage _bufferUsage);
    virtual ~AFK_JigsawImage();

    /* TODO: For the next two functions -- I'm now going around re-
     * acquiring jigsaws for read after having first acquired them
     * for write, and all sorts.  In which case I don't need to re-blit
     * all the same data.  Consider optimising so that data is only
     * copied over to the GL after a write acquire is released?
     */

    /* Acquires this image for the CL. */
    cl_mem acquireForCl(cl_context ctxt, cl_command_queue q, std::vector<cl_event>& o_events);

    /* Releases this image from the CL. */
    void releaseFromCl(const std::vector<AFK_JigsawCuboid>& drawCuboids, cl_command_queue q, const std::vector<cl_event>& eventWaitList);

    /* Binds this image to the GL as a texture. */
    void bindTexture(const std::vector<AFK_JigsawCuboid>& drawCuboids);

    /* Makes sure all events are finished. */
    void waitForAll(void);
};

#endif /* _AFK_JIGSAW_IMAGE_H_ */

