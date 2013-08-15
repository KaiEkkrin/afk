/* AFK (c) Alex Holloway 2013 */

#include "computer.hpp"
#include "core.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "jigsaw.hpp"

#include <climits>
#include <cstring>


#define FIXED_TEST_TEXTURE_DATA 0


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

    case AFK_JIGSAW_4HALF32:
        glInternalFormat                    = GL_RGBA16F;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_FLOAT;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_HALF_FLOAT;
        texelSize                           = sizeof(float) * 2;

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
    piece(afk_vec2<int>(INT_MIN, INT_MIN)), puzzle(INT_MIN)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(const Vec2<int>& _piece, int _puzzle):
    piece(_piece), puzzle(_puzzle)
{
}

bool AFK_JigsawPiece::operator==(const AFK_JigsawPiece& other) const
{
    return (piece == other.piece && puzzle == other.puzzle);
}

bool AFK_JigsawPiece::operator!=(const AFK_JigsawPiece& other) const
{
    return (piece != other.piece || puzzle != other.puzzle);
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece)
{
    return os << "JigsawPiece(piece=" << std::dec << piece.piece << ", puzzle=" << piece.puzzle << ")";
}


/* AFK_JigsawSubRect implementation */

AFK_JigsawSubRect::AFK_JigsawSubRect(int _r, int _c, int _rows):
    r(_r), c(_c), rows(_rows)
{
    columns.store(0);
}

AFK_JigsawSubRect::AFK_JigsawSubRect(const AFK_JigsawSubRect& other):
    r(other.r), c(other.c), rows(other.rows)
{
    columns.store(other.columns.load());
}

AFK_JigsawSubRect AFK_JigsawSubRect::operator=(const AFK_JigsawSubRect& other)
{
    r = other.r;
    c = other.c;
    rows = other.rows;
    columns.store(other.columns.load());
    return *this;
}


/* AFK_Jigsaw implementation */

enum AFK_JigsawPieceGrabStatus AFK_Jigsaw::grabPieceFromRect(unsigned int rect, unsigned int threadId, const AFK_Frame& currentFrame, Vec2<int>& o_uv)
{
    if ((int)threadId >= rects[updateRs][rect].rows)
    {
        /* There aren't enough rows in this rectangle, try the
         * next one.
         */
        return AFK_JIGSAW_RECT_OUT_OF_ROWS;
    }

    /* If I get here, `threadId' is a valid row within the current
     * rectangle.
     * Let's see if I've got enough columns left...
     */
    int row = (int)threadId + rects[updateRs][rect].r;
    if (rowUsage[row] < jigsawSize.v[1])
    {
        /* We have!  Grab one. */
        o_uv = afk_vec2<int>(row, rowUsage[row]++);

        /* If I just gave the rectangle another column, update its columns
         * field to match
         * (I can do this atomically and don't need a lock)
         */
        int rectColumns = o_uv.v[1] - rects[updateRs][rect].c;
        rects[updateRs][rect].columns.compare_exchange_strong(rectColumns, rectColumns + 1);

        return AFK_JIGSAW_RECT_GRABBED;
    }
    else
    {
        /* We ran out of columns, you need to start a new rectangle.
         */
        return AFK_JIGSAW_RECT_OUT_OF_COLUMNS;
    }
}

bool AFK_Jigsaw::startNewRect(const AFK_JigsawSubRect& lastRect, bool startNewRow, const AFK_Frame& currentFrame)
{
    /* Update the column counts with the last rectangle's. */
    columnCounts.push(lastRect.columns.load());

    if (startNewRow ||
        columnCounts.get() > (jigsawSize.v[1] - (lastRect.c + lastRect.columns.load())))
    {
        /* Try starting a new rectangle on the row after this one. */
        int newRow, newRowCount;
        newRow = lastRect.r + lastRect.rows;
        if (newRow == jigsawSize.v[0]) newRow = 0;

        for (newRowCount = 0;
            newRowCount < (int)concurrency &&
                (newRow + newRowCount) < jigsawSize.v[0] &&
                meta->canEvict(rowLastSeen[newRow + newRowCount], currentFrame);
            ++newRowCount);

        if (newRowCount == 0) return false;

        for (int evictRow = newRow; evictRow < newRow + newRowCount; ++evictRow)
        {
            meta->evicted(evictRow);
            rowLastSeen[evictRow] = currentFrame;
            rowUsage[evictRow] = 0;
        }

        AFK_JigsawSubRect newRect(
            newRow, 0, newRow + newRowCount);
        rects[updateRs].push_back(newRect);
    }
    else
    {
        /* I'm going to pack a new rectangle onto the same rows as the existing one.
         * I don't need to do eviction checks, because eviction happens row-at-a-time.
         * This operation also closes down the last rectangle (if you add columns to
         * it you'll trample the new one).
         */
        AFK_JigsawSubRect newRect(
            lastRect.r,
            lastRect.c + lastRect.columns,
            lastRect.rows);
        rects[updateRs].push_back(newRect);

        /* Make sure the current frames are up to date, and that the row usage
         * array is re-aligned to the start of this new rectangle.
         */
        for (int row = newRect.r; row < (newRect.r + newRect.rows); ++row)
        {
            rowLastSeen[row] = currentFrame;
            rowUsage[row] = newRect.c;
        }
    }

    return true;
}

AFK_Jigsaw::AFK_Jigsaw(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    const Vec2<int>& _jigsawSize,
    const AFK_JigsawFormatDescriptor *_format,
    unsigned int _texCount,
    bool _clGlSharing,
    boost::shared_ptr<AFK_JigsawMetadata> _meta,
    unsigned int _concurrency,
    const AFK_Frame& currentFrame):
        format(_format),
        texCount(_texCount),
        pieceSize(_pieceSize),
        jigsawSize(_jigsawSize),
        clGlSharing(_clGlSharing),
        meta(_meta),
        concurrency(_concurrency),
        updateRs(0),
        drawRs(1),
        columnCounts(8, 0)
{
    glTex = new GLuint[texCount];
    clTex = new cl_mem[texCount];

    glGenTextures(texCount, glTex);

    for (unsigned int tex = 0; tex < texCount; ++tex)
    {
        glBindTexture(GL_TEXTURE_2D, glTex[tex]);

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
        Vec4<float> testData[pieceSize.v[0]][pieceSize.v[1]];
        for (int a = 0; a < pieceSize.v[0]; ++a)
        {
            for (int b = 0; b < pieceSize.v[1]; ++b)
            {
                testData[a][b] = afk_vec4<float>(
                    1.0f,
                    (float)a / (float)pieceSize.v[0],
                    (float)b / (float)pieceSize.v[1],
                    0);
            }
        }
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            format.glInternalFormat,
            pieceSize.v[0],
            pieceSize.v[1],
            0,
            format[tex].glFormat,
            format[tex].glDataType,
            &testData[0]);
#else
        glTexStorage2D(GL_TEXTURE_2D, 1, format[tex].glInternalFormat, pieceSize.v[0] * jigsawSize.v[0], pieceSize.v[1] * jigsawSize.v[1]);
#endif
        AFK_GLCHK("AFK_JigSaw texStorage2D")

        cl_int error;
        if (clGlSharing)
        {
            if (afk_core.computer->testVersion(1, 2))
            {
                clTex[tex] = clCreateFromGLTexture(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    GL_TEXTURE_2D,
                    0,
                    glTex[tex],
                    &error);
            }
            else
            {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
                clTex[tex] = clCreateFromGLTexture2D(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    GL_TEXTURE_2D,
                    0,
                    glTex[tex],
                    &error);           
#pragma GCC diagnostic pop
            }
        }
        else
        {
            cl_image_desc imageDesc;
            memset(&imageDesc, 0, sizeof(cl_image_desc));
            imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
            imageDesc.image_width       = pieceSize.v[0] * jigsawSize.v[0];
            imageDesc.image_height      = pieceSize.v[1] * jigsawSize.v[1];

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
                clTex[tex] = clCreateImage2D(
                    ctxt,
                    CL_MEM_READ_WRITE,
                    &format[tex].clFormat,
                    imageDesc.image_width,
                    imageDesc.image_height,
                    imageDesc.image_row_pitch,
                    NULL,
                    &error);
#pragma GCC diagnostic pop
            }
        }
        afk_handleClError(error);
    }

    /* Now that I've got the textures, fill out the jigsaw state. */
    for (int row = 0; row < jigsawSize.v[0]; ++row)
    {
        rowLastSeen.push_back(AFK_Frame());
        rowUsage.push_back(0);
    }

    /* Make a starting update rectangle. */
    if (!startNewRect(
        AFK_JigsawSubRect(0, 0, 0), false, currentFrame))
    {
        throw AFK_Exception("Cannot make starting rectangle");
    }

    changeData = new std::vector<unsigned char>[texCount];
    changeEvents = new std::vector<cl_event>[texCount];
}

AFK_Jigsaw::~AFK_Jigsaw()
{
    for (unsigned int tex = 0; tex < texCount; ++tex)
        clReleaseMemObject(clTex[tex]);

    glDeleteTextures(texCount, glTex);

    delete[] changeEvents;
    delete[] changeData;
    delete[] clTex;
    delete[] glTex;
}

bool AFK_Jigsaw::grab(unsigned int threadId, const AFK_Frame& frame, Vec2<int>& o_uv)
{
    /* Let's see if I can use an existing rectangle. */
    unsigned int rect;
    for (rect = 0; rect < rects[updateRs].size(); ++rect)
    {
        switch (grabPieceFromRect(rect, threadId, frame, o_uv))
        {
        case AFK_JIGSAW_RECT_OUT_OF_ROWS:
            /* This means I'm out of room in the jigsaw. */
            return false;

        case AFK_JIGSAW_RECT_GRABBED:
            /* I got one! */
            return true;

        default:
            /* Keep looking. */
            break;
        }
    }

    /* I ran out of rectangles.  Try to start a new one. */
    boost::unique_lock<boost::mutex> lock(updateRMut);
    if (rect == rects[updateRs].size())
    {
        if (!startNewRect(rects[updateRs][rect-1], true, frame)) return false;
    }

    return grabPieceFromRect(rect, threadId, frame, o_uv) == AFK_JIGSAW_RECT_GRABBED;
}

unsigned int AFK_Jigsaw::getTexCount(void) const
{
    return texCount;
}

Vec2<float> AFK_Jigsaw::getTexCoordST(const AFK_JigsawPiece& piece) const
{
    return afk_vec2<float>(
        (float)piece.piece.v[0] / (float)jigsawSize.v[0],
        (float)piece.piece.v[1] / (float)jigsawSize.v[1]);
}

Vec2<float> AFK_Jigsaw::getPiecePitchST(void) const
{
    return afk_vec2<float>(
        1.0f / (float)jigsawSize.v[0],
        1.0f / (float)jigsawSize.v[1]);
}

cl_mem *AFK_Jigsaw::acquireForCl(cl_context ctxt, cl_command_queue q)
{
    if (clGlSharing)
    {
        AFK_CLCHK(clEnqueueAcquireGLObjects(q, texCount, clTex, 0, 0, 0))
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

void AFK_Jigsaw::releaseFromCl(cl_command_queue q)
{
    if (clGlSharing)
    {
        AFK_CLCHK(clEnqueueReleaseGLObjects(q, texCount, clTex, 0, 0, 0))
    }
    else
    {
#if FIXED_TEST_TEXTURE_DATA
#else
        /* TODO I just did this completely wrong, change it and try again.
         * Maybe make one changeData vector for each tex, and have a
         * utility function that enqueues the rectangle reads (and another
         * that does the rectangle writes) for a single tex?
         */

        /* Work out how much space I need to store all the changed data. */
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            unsigned int requiredChangeEventCount = 0;
            size_t requiredChangeDataSize = 0;
            size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1];

            for (unsigned int rect = 0; rect < rects[drawRs].size(); ++rect)
            {
                size_t rectSizeInBytes = pieceSizeInBytes *
                    rects[drawRs][rect].rows * rects[drawRs][rect].columns.load();
                requiredChangeDataSize += rectSizeInBytes;
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
            for (unsigned int rect = 0; rect < rects[drawRs].size(); ++rect)
            {
                size_t rectSizeInBytes = pieceSizeInBytes *
                    rects[drawRs][rect].rows * rects[drawRs][rect].columns.load();

                size_t origin[3];
                size_t region[3];

                origin[0] = rects[drawRs][rect].r * pieceSize.v[0];
                origin[1] = rects[drawRs][rect].c * pieceSize.v[1];
                origin[2] = 0;

                region[0] = rects[drawRs][rect].rows * pieceSize.v[0];
                region[1] = rects[drawRs][rect].columns.load() * pieceSize.v[1];
                region[2] = 1;

                AFK_CLCHK(clEnqueueReadImage(
                    q, clTex[tex], CL_FALSE, origin, region, 0, 0, &changeData[tex][changeDataOffset], 0, NULL, &changeEvents[tex][changeEvent]))
                ++changeEvent;
                changeDataOffset += rectSizeInBytes;
            }
        }
#endif
    }
}

void AFK_Jigsaw::bindTexture(unsigned int tex)
{
    glBindTexture(GL_TEXTURE_2D, glTex[tex]);

    if (!clGlSharing)
    {
#if FIXED_TEST_TEXTURE_DATA
#else
        /* Wait for the change readbacks to be finished. */
        clWaitForEvents(changeEvents[tex].size(), &changeEvents[tex][0]);

        /* Push all the changed pieces into the GL texture. */
        size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1];
        size_t changeDataOffset = 0;
        for (unsigned int rect = 0; rect < rects[drawRs].size(); ++rect)
        {
            size_t rectSizeInBytes = pieceSizeInBytes *
                rects[drawRs][rect].rows * rects[drawRs][rect].columns.load();

            glTexSubImage2D(
                GL_TEXTURE_2D, 0,
                rects[drawRs][rect].r * pieceSize.v[0],
                rects[drawRs][rect].c * pieceSize.v[1],
                rects[drawRs][rect].rows * pieceSize.v[0],
                rects[drawRs][rect].columns.load() * pieceSize.v[1],
                format[tex].glFormat,
                format[tex].glDataType,
                &changeData[tex][changeDataOffset]);

            changeDataOffset += rectSizeInBytes;
        }
#endif
    }
}

void AFK_Jigsaw::flipRects(const AFK_Frame& currentFrame)
{
    updateRs = (updateRs == 0 ? 1 : 0);
    drawRs = (drawRs == 0 ? 1 : 0);

    /* Clear out the old draw rectangles, and create a new update
     * rectangle following on from the last one.
     */
    rects[updateRs].clear();
    startNewRect(*(rects[drawRs].rbegin()), false, currentFrame);
}


/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    int _pieceCount,
    enum AFK_JigsawFormat *texFormat,
    unsigned int _texCount,
    const AFK_ClDeviceProperties& _clDeviceProps,
    bool _clGlSharing,
    unsigned int _concurrency,
    boost::function<boost::shared_ptr<AFK_JigsawMetadata> (const Vec2<int>&)> _newMeta,
    const AFK_Frame& currentFrame):
        texCount(_texCount),
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        clGlSharing(_clGlSharing),
        concurrency(_concurrency),
        newMeta(_newMeta)
{
    std::cout << "AFK_JigsawCollection: Requested " << std::dec << pieceCount << " pieces of size " << pieceSize << ": " << std::endl;

    /* Figure out the texture formats. */
    for (unsigned int tex = 0; tex < texCount; ++tex)
        format.push_back(AFK_JigsawFormatDescriptor(texFormat[tex]));

    /* Figure out a jigsaw size.  I want the rows to always be a
     * round multiple of `concurrency' to avoid breaking rectangles
     * apart.
     * For this I need to try all the formats: I stop testing
     * when any one of the formats fails, because all the
     * jigsaw textures need to be identical aside from their
     * texels
     */
    Vec2<int> startingJigsawSize = afk_vec2<int>(concurrency, concurrency);
    jigsawSize = startingJigsawSize;
    GLuint glProxyTex[texCount];
    glGenTextures(texCount, glProxyTex);
    GLint texWidth;
    bool dimensionsOK = true;
    for (Vec2<int> testJigsawSize = jigsawSize;
        dimensionsOK && jigsawSize.v[0] * jigsawSize.v[1] < pieceCount;
        testJigsawSize += startingJigsawSize)
    {
        dimensionsOK = (
            testJigsawSize.v[0] <= (int)_clDeviceProps.image2DMaxWidth &&
            testJigsawSize.v[1] <= (int)_clDeviceProps.image2DMaxHeight);

        /* Try to make pretend textures of the current jigsaw size */
        for (unsigned int tex = 0; tex < texCount && dimensionsOK; ++tex)
        {
            dimensionsOK &= ((testJigsawSize.v[0] * testJigsawSize.v[1] * pieceSize.v[0] * pieceSize.v[1] * format[tex].texelSize) < _clDeviceProps.maxMemAllocSize);
            if (!dimensionsOK) break;

            glBindTexture(GL_PROXY_TEXTURE_2D, glProxyTex[tex]);
            glTexImage2D(
                GL_PROXY_TEXTURE_2D,
                0,
                format[tex].glInternalFormat,
                pieceSize.v[0] * testJigsawSize.v[0],
                pieceSize.v[1] * testJigsawSize.v[1],
                0,
                format[tex].glFormat,
                format[tex].glDataType,
                NULL);

            /* See if it worked */
            glGetTexLevelParameteriv(
                GL_PROXY_TEXTURE_2D,
                0,
                GL_TEXTURE_WIDTH,
                &texWidth);

            dimensionsOK &= (texWidth != 0);
        }

        if (dimensionsOK) jigsawSize = testJigsawSize;
    }

    glDeleteTextures(texCount, glProxyTex);
    glGetError(); /* Throw away any error that might have popped up */

    /* Update the dimensions and actual piece count to reflect what I found */
    int jigsawCount = pieceCount / (jigsawSize.v[0] * jigsawSize.v[1]) + 1;
    pieceCount = jigsawCount * jigsawSize.v[0] * jigsawSize.v[1];

    std::cout << "AFK_JigsawCollection: Making " << jigsawCount << " jigsaws with " << jigsawSize << " pieces each (actually " << pieceCount << " pieces)" << std::endl;

    /* TODO consider making one as a spare and filling the box up
     * as an async task?
     */
    for (int j = 0; j < jigsawCount; ++j)
    {
        puzzles.push_back(new AFK_Jigsaw(
            ctxt,
            pieceSize,
            jigsawSize,
            &format[0],
            texCount,
            clGlSharing,
            newMeta(jigsawSize),
            concurrency,
            currentFrame));
    }
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
    for (std::vector<AFK_Jigsaw*>::iterator pIt = puzzles.begin();
        pIt != puzzles.end(); ++pIt)
    {
        delete *pIt;
    }
}

int AFK_JigsawCollection::getPieceCount(void) const
{
    return pieceCount;
}

AFK_JigsawPiece AFK_JigsawCollection::grab(unsigned int threadId, const AFK_Frame& frame)
{
    Vec2<int> uv;

    int puzzle;
    for (puzzle = 0; puzzle < (int)puzzles.size(); ++puzzle)
    {
        if (puzzles[puzzle]->grab(threadId, frame, uv))
        {
            return AFK_JigsawPiece(uv, puzzle);
        }
    }

    /* If we get here, I need to add a new jigsaw.
     * TODO This operation needs an OpenCL context, which I
     * currently don't have in the enumerator threads.
     * More plumbing will be required to add one.
     * For now I'm just going to hope the original
     * jigsaw(s) don't run out of room, and throw here.
     */
    throw AFK_Exception("grab() ran out of jigsaws");
#if 0
    boost::unique_lock<boost::mutex> lock(mut);
    if (puzzle == (int)puzzles.size())
    {
        AFK_Jigsaw *newJigsaw = new AFK_Jigsaw(
            ctxt,
            pieceSize,
            jigsawSize,
            &format[0],
            texCount,
            clGlSharing,
            newMeta(jigsawSize),
            concurrency,
            currentFrame);

        puzzles.push_back(newJigsaw);
    }
    if (!puzzles[puzzle]->grab(threadId, frame, uv))
        throw AFK_Exception("Unable to grab piece from new jigsaw");
    return AFK_JigsawPiece(uv, puzzle);
#endif
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(const AFK_JigsawPiece& piece) const
{
    if (piece == AFK_JigsawPiece()) throw AFK_Exception("AFK_JigsawCollection: Called getPuzzle() with the null piece");
    return puzzles[piece.puzzle];
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(int puzzle) const
{
    return puzzles[puzzle];
}

