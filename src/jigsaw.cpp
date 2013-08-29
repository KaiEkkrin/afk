/* AFK (c) Alex Holloway 2013 */

#include "computer.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "jigsaw.hpp"

#include <climits>
#include <cstring>
#include <iostream>


#define FIXED_TEST_TEXTURE_DATA 0

#define GRAB_DEBUG 0
#define CUBOID_DEBUG 0


/* AFK_JigsawFormatDescriptor implementation */

AFK_JigsawFormatDescriptor::AFK_JigsawFormatDescriptor(enum AFK_JigsawFormat e)
{
    switch (e)
    {
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
        texelSize                           = sizeof(unsigned short);
        break;

    case AFK_JIGSAW_101010A2:
        glInternalFormat                    = GL_RGB10;
        glFormat                            = GL_RGB;
        glDataType                          = GL_UNSIGNED_INT_10_10_10_2;
        clFormat.image_channel_order        = CL_RGB;
        clFormat.image_channel_data_type    = CL_UNORM_INT_101010;
        texelSize                           = sizeof(unsigned int);
        break;

    case AFK_JIGSAW_4FLOAT8_UNORM:
        glInternalFormat                    = GL_RGBA8;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_UNSIGNED_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_UNORM_INT8;
        texelSize                           = sizeof(unsigned char) * 4;
        break;

    /* TODO cl_gl sharing seems to barf with this one ... */
    case AFK_JIGSAW_4FLOAT8_SNORM:
        glInternalFormat                    = GL_RGBA8_SNORM;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_SNORM_INT8;
        texelSize                           = sizeof(unsigned char) * 4;
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

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece)
{
    return os << "JigsawPiece(u=" << std::dec << piece.u << ", v=" << piece.v << ", w=" << piece.w << ", puzzle=" << piece.puzzle << ")";
}


/* AFK_JigsawCuboid implementation */

AFK_JigsawCuboid::AFK_JigsawCuboid(int _r, int _c, int _s, int _rows):
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
    AFK_Frame& o_timestamp)
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
    int slice = (int)threadId + cuboid.s;
    if (rowUsage[row][slice] < jigsawSize.v[1])
    {
        /* We have!  Grab one. */
        o_uvw = afk_vec3<int>(row, rowUsage[row][slice]++, slice);
        o_timestamp = rowTimestamp[row][slice];

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
        if ((lastCuboid.r + lastCuboid.rows + newRowCount) < jigsawSize.v[0])
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
        if ((newSlice <= sweepPosition.v[2] && sweepPosition.v[2] < (newSlice + newSliceCount)) &&
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
    if (jigsawSize.v[2] > 1)
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
        if ((sweepRowCmp - nextFreeRow.v[0]) < (jigsawSize.v[0] / concurrency))
        {
            sweep(getSweepTarget(afk_vec2<int>(sweepRowCmp, nextFreeRow.v[1])), currentFrame);
        }
    }
}

AFK_Jigsaw::AFK_Jigsaw(
    cl_context ctxt,
    const Vec3<int>& _pieceSize,
    const Vec3<int>& _jigsawSize,
    const AFK_JigsawFormatDescriptor *_format,
    GLuint _texTarget,
    unsigned int _texCount,
    bool _clGlSharing,
    unsigned int _concurrency):
        format(_format),
        texTarget(_texTarget),
        texCount(_texCount),
        pieceSize(_pieceSize),
        jigsawSize(_jigsawSize),
        clGlSharing(_clGlSharing),
        concurrency(_concurrency),
        updateCs(0),
        drawCs(1),
        columnCounts(8, 0)
{
    glTex = new GLuint[texCount];
    clTex = new cl_mem[texCount];

    glGenTextures(texCount, glTex);

    for (unsigned int tex = 0; tex < texCount; ++tex)
    {
        glBindTexture(texTarget, glTex[tex]);

        /* Next debug: fill this thing out with some test data and
         * bludgeon the texture sampler into working correctly.  I'm
         * sure it isn't.
         * I'm keeping this around in case it becomes useful again
         * at some point.
         */
#if FIXED_TEST_TEXTURE_DATA
        /* This is awful, I'm going to suddenly make a huge assumption
         * about the texture format -- but it's for a good debug cause
         * honest! :P
         * This code assumes jigsawSize to be {1, 1} for expediency
         */
        Vec4<float> testData[pieceSize.v[0]][pieceSize.v[1]][pieceSize.v[2]];
        for (int a = 0; a < pieceSize.v[0]; ++a)
        {
            for (int b = 0; b < pieceSize.v[1]; ++b)
            {
                for (int c = 0; c < pieceSize.v[2]; ++c)
                {
                    testData[a][b][c] = afk_vec4<float>(
                        1.0f,
                        (float)a / (float)pieceSize.v[0],
                        (float)b / (float)pieceSize.v[1],
                        (float)c / (float)pieceSize.v[2]);
                }
            }
        }
        switch (texTarget)
        {
        case GL_TEXTURE_2D:
            glTexImage2D(
                texTarget,
                0,
                format.glInternalFormat,
                pieceSize.v[0],
                pieceSize.v[1],
                0,
                format[tex].glFormat,
                format[tex].glDataType,
                &testData[0]);
            break;

        case GL_TEXTURE_3D:
            glTexImage3D(
                texTarget,
                0,
                format.glInternalFormat,
                pieceSize.v[0],
                pieceSize.v[1],
                pieceSize.v[2],
                format[tex].glFormat,
                format[tex].glDataType,
                &testData[0]);
            break;

        default:
            throw AFK_Exception("Unrecognised texTarget");
        }
#else
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
#endif
        AFK_GLCHK("AFK_JigSaw texStorage")

        cl_int error;
        if (clGlSharing)
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
        }
        else
        {
            cl_image_desc imageDesc;
            memset(&imageDesc, 0, sizeof(cl_image_desc));
            imageDesc.image_type        = (texTarget == GL_TEXTURE_2D ? CL_MEM_OBJECT_IMAGE2D : CL_MEM_OBJECT_IMAGE3D);
            imageDesc.image_width       = pieceSize.v[0] * jigsawSize.v[0];
            imageDesc.image_height      = pieceSize.v[1] * jigsawSize.v[1];
            imageDesc.image_depth       = pieceSize.v[2] * jigsawSize.v[2];

            if (afk_core.computer->testVersion(1, 2))
            {
                clTex[tex] = clCreateImage(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &format[tex].clFormat,
                    &imageDesc,
                    NULL,
                    &error);
            }
            else
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                switch (texTarget)
                {
                case GL_TEXTURE_2D:
                    clTex[tex] = clCreateImage2D(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        &format[tex].clFormat,
                        imageDesc.image_width,
                        imageDesc.image_height,
                        imageDesc.image_row_pitch,
                        NULL,
                        &error);
                    break;

                case GL_TEXTURE_3D:
                    clTex[tex] = clCreateImage3D(
                        ctxt,
                        CL_MEM_READ_WRITE,
                        &format[tex].clFormat,
                        imageDesc.image_width,
                        imageDesc.image_height,
                        imageDesc.image_depth,
                        imageDesc.image_row_pitch,
                        imageDesc.image_slice_pitch,
                        NULL,
                        &error);
                    break;

                default:
                    throw AFK_Exception("Unrecognised texTarget");
                }
#pragma GCC diagnostic pop
            }
        }
        afk_handleClError(error);
    }

    /* Now that I've got the textures, fill out the jigsaw state. */
    rowTimestamp = new AFK_Frame[jigsawSize.v[0]][jigsawSize.v[2]];
    rowUsage = new int[jigsawSize.v[0]][jigsawSize.v[2]];

    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
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

    changeData = new std::vector<unsigned char>[texCount];
    changeEvents = new std::vector<cl_event>[texCount];
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

    glDeleteTextures(texCount, glTex);

    delete[][] rowUsage;
    delete[][] rowTimestamp;

    delete[] changeEvents;
    delete[] changeData;
    delete[] clTex;
    delete[] glTex;
}

bool AFK_Jigsaw::grab(unsigned int threadId, Vec2<int>& o_uvw, AFK_Frame& o_timestamp)
{
    /* Let's see if I can use an existing cuboid. */
    unsigned int cI;
    for (cI = 0; cI < cuboids[updateCs].size(); ++cI)
    {
        switch (grabPieceFromCuboid(cuboids[updateCs][cI], threadId, o_uvw, o_timestamp))
        {
        case AFK_JIGSAW_CUBOID_OUT_OF_SPACE:
            /* This means I'm out of room in the jigsaw. */
            return false;

        case AFK_JIGSAW_CUBOID_GRABBED:
            /* I got one! */
            return true;

        default:
            /* Keep looking. */
            break;
        }
    }

    /* I ran out of cuboids.  Try to start a new one. */
    if (cI == 0) return false;

    boost::unique_lock<boost::mutex> lock(updateCMut);
    if (cI == cuboids[updateCs].size())
    {
        if (!startNewCuboid(cuboids[updateCs][cI-1], true)) return false;
    }

    return grabPieceFromCuboid(cuboids[updateCs][cI], threadId, o_uvw, o_timestamp) == AFK_JIGSAW_CUBOID_GRABBED;
}

AFK_Frame AFK_Jigsaw::getTimestamp(const AFK_JigsawPiece& piece) const
{
    return rowTimestamp[piece.u][piece.w];
}

unsigned int AFK_Jigsaw::getTexCount(void) const
{
    return texCount;
}

Vec3<float> AFK_Jigsaw::getTexCoordST(const AFK_JigsawPiece& piece) const
{
    return afk_vec3<float>(
        (float)piece.u / (float)jigsawSize.v[0],
        (float)piece.v / (float)jigsawSize.v[1],
        (float)piece.w / (float)jigsawSize.v[2]);
}

Vec3<float> AFK_Jigsaw::getPiecePitchST(void) const
{
    return afk_vec3<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1],
        1.0f / (float)jigsawSize.v[2]);
}

cl_mem *AFK_Jigsaw::acquireForCl(cl_context ctxt, cl_command_queue q, cl_event *o_event)
{
    if (clGlSharing)
    {
        AFK_CLCHK(clEnqueueAcquireGLObjects(q, texCount, clTex, 0, 0, o_event))
    }
    else
    {
        /* Make sure the change state is reset so that I can start
         * accumulating new changes
         */
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            changeData[tex].clear();
            changeEvents[tex].clear();
        }
    }
    return clTex;
}

void AFK_Jigsaw::releaseFromCl(cl_command_queue q, cl_uint eventsInWaitList, const cl_event *eventWaitList)
{
    if (clGlSharing)
    {
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            changeEvents[tex].resize(1);
            AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clTex[tex], eventsInWaitList, eventWaitList, &changeEvents[tex][0]))
        }
    }
    else
    {
#if FIXED_TEST_TEXTURE_DATA
#else
        /* Work out how much space I need to store all the changed data. */
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            unsigned int requiredChangeEventCount = 0;
            size_t requiredChangeDataSize = 0;
            size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2];

            for (unsigned int cI = 0; cI < cuboids[drawCs].size(); ++cI)
            {
                size_t cuboidSizeInBytes = pieceSizeInBytes *
                    cuboids[drawCs][cI].rows *
                    cuboids[drawCs][cI].columns.load() *
                    cuboids[drawCs][cI].slices;

                if (cuboidSizeInBytes == 0) continue;

                requiredChangeDataSize += cuboidSizeInBytes;
                ++requiredChangeEventCount;
            }

            changeEvents[tex].resize(requiredChangeEventCount);
            changeData[tex].resize(requiredChangeDataSize);

            /* Now, read back all the pieces, appending the data to `changeData'
             * in order.
             * So long as `bindTexture' writes them in the same order everything
             * will be okay!
             */
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

                AFK_CLCHK(clEnqueueReadImage(
                    q, clTex[tex], CL_FALSE, origin, region, 0, 0, &changeData[tex][changeDataOffset],
                        eventsInWaitList, eventWaitList, &changeEvents[tex][changeEvent]))
                ++changeEvent;
                changeDataOffset += cuboidSizeInBytes;
            }
        }
#endif
    }
}

void AFK_Jigsaw::bindTexture(unsigned int tex)
{
    glBindTexture(texTarget, glTex[tex]);

    if (!clGlSharing)
    {
#if FIXED_TEST_TEXTURE_DATA
#else
        /* Wait for the change readback to be finished. */
        if (changeEvents[tex].size() == 0) return;

        AFK_CLCHK(clWaitForEvents(changeEvents[tex].size(), &changeEvents[tex][0]))
        for (unsigned int e = 0; e < changeEvents[tex].size(); ++e)
        {
            AFK_CLCHK(clReleaseEvent(changeEvents[tex][e]))
        }

        changeEvents[tex].clear();

        /* Push all the changed pieces into the GL texture. */
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
#endif
    }
}

#define FLIP_DEBUG 0

void AFK_Jigsaw::flipCuboids(const AFK_Frame& currentFrame)
{
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

