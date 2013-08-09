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
        glFormat                            = GL_R;
        glDataType                          = GL_FLOAT;
        clFormat.image_channel_order        = CL_R;
        clFormat.image_channel_data_type    = CL_FLOAT;
        texelSize                           = sizeof(float);
        break;

    case AFK_JIGSAW_4FLOAT8:
        glInternalFormat                    = GL_RGBA;
        glFormat                            = GL_RGBA;
        glDataType                          = GL_UNSIGNED_BYTE;
        clFormat.image_channel_order        = CL_RGBA;
        clFormat.image_channel_data_type    = CL_UNSIGNED_INT8;
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


/* AFK_Jigsaw implementation */
AFK_Jigsaw::AFK_Jigsaw(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    const Vec2<int>& _jigsawSize,
    const AFK_JigsawFormatDescriptor& _format,
    bool _clGlSharing,
    unsigned char *zeroMem):
        pieceSize(_pieceSize),
        jigsawSize(_jigsawSize),
        format(_format),
        clGlSharing(_clGlSharing)
{
    glGenTextures(1, &glTex);
    glBindTexture(GL_TEXTURE_2D, glTex);

    /* TODO Next debug: fill this thing out with some test data and
     * bludgeon the texture sampler into working correctly.  I'm
     * sure it isn't.
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
        format.glFormat /* TODO See http://www.opengl.org/discussion_boards/showthread.php/177713-Problem-with-HDR-rendering : looks like I need GL_R / GL_RGB / GL_RGBA here, and not something with a `32F' or similar on the end */,
        format.glDataType,
        &testData[0]);
#else
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        format.glInternalFormat,
        pieceSize.v[0] * jigsawSize.v[0],
        pieceSize.v[1] * jigsawSize.v[1],
        0,
        format.glFormat,
        format.glDataType,
        zeroMem);
    //glTexStorage2D(GL_TEXTURE_2D, 1, format.glInternalFormat, pieceSize.v[0] * jigsawSize.v[0], pieceSize.v[1] * jigsawSize.v[1]);
#endif
    AFK_GLCHK("AFK_JigSaw texStorage2D")

    cl_int error;
    if (clGlSharing)
    {
        if (afk_core.computer->testVersion(1, 2))
        {
            clTex = clCreateFromGLTexture(
                ctxt,
                CL_MEM_WRITE_ONLY, /* TODO Ooh!  Look at the docs for this function: will it turn out I can read/write the same texture in one compute kernel after all? */
                GL_TEXTURE_2D,
                0,
                glTex,
                &error);
        }
        else
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            clTex = clCreateFromGLTexture2D(
                ctxt,
                CL_MEM_WRITE_ONLY,
                GL_TEXTURE_2D,
                0,
                glTex,
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
        // "must be 0 if host_ptr is null"
        //imageDesc.image_row_pitch   = imageDesc.image_width * _texelSize;

        if (afk_core.computer->testVersion(1, 2))
        {
            clTex = clCreateImage(
                ctxt,
                CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, /* TODO As above! */
                &format.clFormat,
                &imageDesc,
                NULL,
                &error);
        }
        else
        {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            clTex = clCreateImage2D(
                ctxt,
                CL_MEM_WRITE_ONLY,
                &format.clFormat,
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

AFK_Jigsaw::~AFK_Jigsaw()
{
    clReleaseMemObject(clTex);
    glDeleteTextures(1, &glTex);
}

Vec2<float> AFK_Jigsaw::getTexCoordST(const AFK_JigsawPiece& piece) const
{
    float jigsawSizeX = (float)pieceSize.v[0] * (float)jigsawSize.v[0];
    float jigsawSizeZ = (float)pieceSize.v[1] * (float)jigsawSize.v[1];
    return afk_vec2<float>(
        (float)piece.piece.v[0] / jigsawSizeX,
        (float)piece.piece.v[1] / jigsawSizeZ);
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
        AFK_CLCHK(clEnqueueAcquireGLObjects(q, 1, &clTex, 0, 0, 0))
    }
    return &clTex;
}

void AFK_Jigsaw::pieceChanged(const Vec2<int>& piece)
{
    if (!clGlSharing) changedPieces.push_back(piece);
}

void AFK_Jigsaw::releaseFromCl(cl_command_queue q)
{
    if (clGlSharing)
    {
        AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clTex, 0, 0, 0))
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
        size_t pieceSizeInBytes = format.texelSize * pieceSize.v[0] * pieceSize.v[1];
        size_t requiredChangedPiecesSize = (pieceSizeInBytes * changedPieces.size());
        if (changes.size() < requiredChangedPiecesSize)
            changes.resize(requiredChangedPiecesSize);

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

            AFK_CLCHK(clEnqueueReadImage(q, clTex, CL_TRUE, origin, region, 0 /* pieceSize.v[0] * format.texelSize */, 0, &changes[s * pieceSizeInBytes], 0, NULL, NULL))
        }
#endif
    }
}

void AFK_Jigsaw::bindTexture(void)
{
    glBindTexture(GL_TEXTURE_2D, glTex);

    if (!clGlSharing)
    {
#if FIXED_TEST_TEXTURE_DATA
#else
        /* Push all the changed pieces into the GL texture. */
        size_t pieceSizeInBytes = format.texelSize * pieceSize.v[0] * pieceSize.v[1];
        for (unsigned int s = 0; s < changedPieces.size(); ++s)
        {
            glTexSubImage2D(
                GL_TEXTURE_2D, 0,
                changedPieces[s].v[0] * pieceSize.v[0],
                changedPieces[s].v[1] * pieceSize.v[1],
                pieceSize.v[0],
                pieceSize.v[1],
                format.glFormat,
                format.glDataType,
                &changes[s * pieceSizeInBytes]);
        }

        changedPieces.clear();
        changes.clear();
#endif
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
}


/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    int _pieceCount,
    enum AFK_JigsawFormat _texFormat,
    const AFK_ClDeviceProperties& _clDeviceProps,
    bool _clGlSharing):
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        format(_texFormat),
        clGlSharing(_clGlSharing),
        box(_pieceCount)
{
    std::cout << "AFK_JigsawCollection: With " << std::dec << pieceCount << " pieces of size " << pieceSize << ": " << std::endl;

    /* Figure out a jigsaw size. */
    jigsawSize = afk_vec2<int>(1, 1);
    GLuint glProxyTex;
    glGenTextures(1, &glProxyTex);
    glBindTexture(GL_PROXY_TEXTURE_2D, glProxyTex);
    GLint texWidth;
    bool dimensionsOK = true;
    for (Vec2<int> testJigsawSize = jigsawSize;
        dimensionsOK && jigsawSize.v[0] * jigsawSize.v[1] < pieceCount;
        testJigsawSize = testJigsawSize * 2)
    {
        /* Try to make a pretend texture of the current jigsaw size */
        glTexImage2D(
            GL_PROXY_TEXTURE_2D,
            0,
            format.glInternalFormat,
            pieceSize.v[0] * testJigsawSize.v[0],
            pieceSize.v[1] * testJigsawSize.v[1],
            0,
            format.glFormat,
            format.glDataType,
            NULL);

        /* See if it worked */
        glGetTexLevelParameteriv(
            GL_PROXY_TEXTURE_2D,
            0,
            GL_TEXTURE_WIDTH,
            &texWidth);

        dimensionsOK = (texWidth != 0 &&
            testJigsawSize.v[0] <= 2 &&
            testJigsawSize.v[0] <= (int)_clDeviceProps.image2DMaxWidth &&
            testJigsawSize.v[1] <= (int)_clDeviceProps.image2DMaxHeight &&
            (testJigsawSize.v[0] * testJigsawSize.v[1] * pieceSize.v[0] * pieceSize.v[1] * format.texelSize) < _clDeviceProps.maxMemAllocSize);
        if (dimensionsOK) jigsawSize = testJigsawSize;
    }

    glDeleteTextures(1, &glProxyTex);

    int jigsawCount = pieceCount / (jigsawSize.v[0] * jigsawSize.v[1]) + 1;

    std::cout << "AFK_JigsawCollection: Making " << jigsawCount << " jigsaws with " << jigsawSize << " pieces each" << std::endl;

    /* Make a large enough buffer to fill one of the jigsaws from */
    zeroMemSize =
        pieceSize.v[0] * pieceSize.v[1] *
        jigsawSize.v[0] * jigsawSize.v[1] *
        format.texelSize;
    zeroMem = new unsigned char[zeroMemSize];
    memset(zeroMem, 0, zeroMemSize);

    /* TODO consider making one as a spare and filling the box up
     * as an async task?
     */
    for (int j = 0; j < jigsawCount; ++j)
    {
        puzzles.push_back(new AFK_Jigsaw(
            ctxt,
            pieceSize,
            jigsawSize,
            format,
            clGlSharing,
            zeroMem));

        for (int x = 0; x < jigsawSize.v[0]; ++x)
            for (int y = 0; y < jigsawSize.v[1]; ++y)
                box.push(AFK_JigsawPiece(afk_vec2<int>(x, y), j));
    }
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
    delete[] zeroMem;

    for (std::vector<AFK_Jigsaw*>::iterator pIt = puzzles.begin();
        pIt != puzzles.end(); ++pIt)
    {
        delete *pIt;
    }
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

