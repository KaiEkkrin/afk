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

AFK_JigsawSubRect():
    r(0), c(0), rows(0), columns(0)
{
}


/* AFK_Jigsaw implementation */

enum AFK_JigsawPieceGrabStatus AFK_Jigsaw::grabPieceFromRect(unsigned int rect, unsigned int threadId, const AFK_Frame& currentFrame)
{
    /* TODO This is wrong too.  I should not be trying to do the
     * below business of grabbing new rows of rectangle whenever
     * a thread ID I haven't seen before comes in, because if I'm
     * doing that, when a thread runs out of columns instead the
     * operation to make a new rectangle will cut off growth room
     * for the first one.
     * Instead, I should have `concurrency' as a parameter, and
     * use it to establish the row count for each rectangle
     * upon initialisation.
     */

    if (threadId >= rects[updateRs][rect].rows)
    {
        /* Try to grab enough rows for this rectangle to match the
         * thread ID.
         * This is clearly a contended operation because I'll be
         * running across the rows for any intermediate threads.
         */
        boost::unique_lock<boost::mutex> lock(rects[updateRs].mut);
        if (threadId >= rects[updateRs][rect].rows)
        {
            for (int newRowOffset = rects[updateRs][rect].rows; newRowOffset <= threadId; ++newRowOffset)
            {
                int row = rects[updateRs][rect].r + newRowOffset;

                if (row == jigsawSize.v[0])
                    return AFK_JIGSAW_RECT_WRAPPED;

                /* Obviously if we've never seen this row before we can use it.
                 * Otherwise...
                 */
                if (!rowLastSeen[row] == AFK_Frame())
                {
                    /* Can we evict what's currently there? */
                    if (meta->canEvict(rowLastSeen[newRow]))
                    {
                        meta->evicted(newRow);
                        rowLastSeen[newRow] = currentFrame;

                        /* We'd better occupy that (even if it's not this thread's) */
                        rects[updateRect].rows = newRowOffset;
                    }
                    else
                    {
                        /* I can't make the rectangle any bigger, you will
                         * have to use a different jigsaw instead.
                         */
                        return AFK_JIGSAW_RECT_OUT_OF_ROWS;
                    }
                }
            }
        }
    }

    /* If I get here, `threadId' is a valid row within the current
     * rectangle.
     * Let's see if I've got enough columns left...
     */
    int row = threadId + rects[updateRs][rect].r;
    if (rowUsage[row] < jigsawSize.v[1])
    {
        /* We have!  Grab one. */
        o_uv = afk_vec2<int>(row, rowUsage[row]++);

        /* If I just gave the rectangle another column, update its columns
         * field to match
         * (I can do this atomically and don't need the lock)
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

AFK_Jigsaw::AFK_Jigsaw(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    const Vec2<int>& _jigsawSize,
    const AFK_JigsawFormatDescriptor *_format,
    unsigned int _texCount,
    bool _clGlSharing,
    boost::shared_ptr<AFK_JigsawMetadata> _meta):
        format(_format),
        texCount(_texCount),
        pieceSize(_pieceSize),
        jigsawSize(_jigsawSize),
        clGlSharing(_clGlSharing),
        meta(_meta),
        updateRect(0),
        drawRect(1)
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
}

AFK_Jigsaw::~AFK_Jigsaw()
{
    for (unsigned int tex = 0; tex < texCount; ++tex)
        clReleaseMemObject(clTex[tex]);

    glDeleteTextures(texCount, glTex);

    delete[] changes;
    delete[] clTex;
    delete[] glTex;
}

bool AFK_Jigsaw::grab(unsigned int threadId, const AFK_Frame& frame, Vec2<int>& o_uv)
{
    /* TODO: The below is wrong too.  I need to iterate through the
     * possible rectangles, of which there are several.  Maybe I should
     * pull the single sub-rectangle out into a subclass for ease.
     * (Maybe I did it already ...)
     */

    if (threadId >= rects[updateRect].rows)
    {
        /* I need to try to get more rows into this rectangle. */
        boost::unique_lock<boost::mutex> lock(rects[updateRect].mut);
        if (threadId >= rects[updateRect].rows)
        {
        }
    }

    

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
        changedPieces.clear();
        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            changes[tex].clear();
        }
        changeEvents.clear();
    }
    return clTex;
}

void AFK_Jigsaw::pieceChanged(const Vec2<int>& piece)
{
    if (!clGlSharing) changedPieces.push_back(piece);
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
        /* Read back all the changed pieces.
         * TODO It would be best to enqueue these as async read-backs
         * as they get reported to me, and then wait on the read-backs
         * here.  But that's harder, so I'll do it like this first
         */
        if (changeEvents.size() < changedPieces.size())
            changeEvents.resize(changedPieces.size());

        for (unsigned int tex = 0; tex < texCount; ++tex)
        {
            size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1];
            size_t requiredChangedPiecesSize = (pieceSizeInBytes * changedPieces.size());
            if (changes[tex].size() < requiredChangedPiecesSize)
                changes[tex].resize(requiredChangedPiecesSize);

            for (unsigned int s = 0; s < changedPieces.size(); ++s)
            {
                size_t origin[3];
                size_t region[3];

                origin[0] = changedPieces[s].v[0] * pieceSize.v[0];
                origin[1] = changedPieces[s].v[1] * pieceSize.v[1];
                origin[2] = 0;

                region[0] = pieceSize.v[0];
                region[1] = pieceSize.v[1];
                region[2] = 1;

                AFK_CLCHK(clEnqueueReadImage(q, clTex[tex], CL_FALSE, origin, region, 0 /* pieceSize.v[0] * format.texelSize */, 0, &changes[tex][s * pieceSizeInBytes], 0, NULL, &changeEvents[s]))
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
        clWaitForEvents(changeEvents.size(), &changeEvents[0]);

        /* Push all the changed pieces into the GL texture. */
        size_t pieceSizeInBytes = format[tex].texelSize * pieceSize.v[0] * pieceSize.v[1];
        for (unsigned int s = 0; s < changedPieces.size(); ++s)
        {
            glTexSubImage2D(
                GL_TEXTURE_2D, 0,
                changedPieces[s].v[0] * pieceSize.v[0],
                changedPieces[s].v[1] * pieceSize.v[1],
                pieceSize.v[0],
                pieceSize.v[1],
                format[tex].glFormat,
                format[tex].glDataType,
                &changes[tex][s * pieceSizeInBytes]);
        }
#endif
    }

    /* TODO Move this into world -- it'll be texture specific */
#if 0
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
#endif
}


/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    int _pieceCount,
    enum AFK_JigsawFormat *texFormat,
    unsigned int _texCount,
    const AFK_ClDeviceProperties& _clDeviceProps,
    bool _clGlSharing):
        texCount(_texCount),
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        clGlSharing(_clGlSharing),
        box(_pieceCount)
{
    std::cout << "AFK_JigsawCollection: Requested " << std::dec << pieceCount << " pieces of size " << pieceSize << ": " << std::endl;

    /* Figure out the texture formats. */
    for (unsigned int tex = 0; tex < texCount; ++tex)
        format.push_back(AFK_JigsawFormatDescriptor(texFormat[tex]));

    /* Figure out a jigsaw size. 
     * For this I need to try all the formats: I stop testing
     * when any one of the formats fails, because all the
     * jigsaw textures need to be identical aside from their
     * texels
     */
    jigsawSize = afk_vec2<int>(1, 1);
    GLuint glProxyTex[texCount];
    glGenTextures(texCount, glProxyTex);
    GLint texWidth;
    bool dimensionsOK = true;
    for (Vec2<int> testJigsawSize = jigsawSize;
        dimensionsOK && jigsawSize.v[0] * jigsawSize.v[1] < pieceCount;
        testJigsawSize = testJigsawSize * 2)
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
            clGlSharing));

        for (int x = 0; x < jigsawSize.v[0]; ++x)
            for (int y = 0; y < jigsawSize.v[1]; ++y)
                box.push(AFK_JigsawPiece(afk_vec2<int>(x, y), j));
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

AFK_JigsawPiece AFK_JigsawCollection::grab(void)
{
    AFK_JigsawPiece piece;
    if (!box.pop(piece))
    {
        /* TODO Add another puzzle to the collection. */
        throw AFK_Exception("Ran out of pieces");
    }

    return piece;
}

void AFK_JigsawCollection::replace(AFK_JigsawPiece piece)
{
    if (piece == AFK_JigsawPiece()) throw AFK_Exception("AFK_JigsawCollection: Called replace() with the null piece");
    box.push(piece);
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

