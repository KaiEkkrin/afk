/* AFK (c) Alex Holloway 2013 */

#include "computer.hpp"
#include "display.hpp"
#include "exception.hpp"
#include "jigsaw.hpp"

#include <climits>
#include <cstring>


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
    GLenum _glTexFormat,
    const cl_image_format& _clTexFormat,
    size_t _texelSize,
    bool _clGlSharing,
    unsigned char *zeroMem):
        pieceSize(_pieceSize),
        jigsawSize(_jigsawSize),
        glTexFormat(_glTexFormat),
        clTexFormat(_clTexFormat),
        texelSize(_texelSize),
        clGlSharing(_clGlSharing)
{
    glGenTextures(1, &glTex);
    glBindTexture(GL_TEXTURE_2D, glTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, _glTexFormat, pieceSize.v[0] * jigsawSize.v[0], pieceSize.v[1] * jigsawSize.v[1]);
    AFK_GLCHK("AFK_JigSaw texStorage2D")

    cl_int error;
    if (clGlSharing)
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
        cl_image_desc imageDesc;
        memset(&imageDesc, 0, sizeof(cl_image_desc));
        imageDesc.image_type        = CL_MEM_OBJECT_IMAGE2D;
        imageDesc.image_width       = pieceSize.v[0] * jigsawSize.v[0];
        imageDesc.image_height      = pieceSize.v[1] * jigsawSize.v[1];
        // "must be 0 if host_ptr is null"
        //imageDesc.image_row_pitch   = imageDesc.image_width * _texelSize;

        /* TODO This segfaults.  I've got a feeling it might not be
         * implemented on Nvidia, and I need to use the old
         * clCreateImage2D() instead ...?
         */
        clTex = clCreateImage(
            ctxt,
            CL_MEM_WRITE_ONLY | CL_MEM_HOST_READ_ONLY, /* TODO As above! */
            &clTexFormat,
            &imageDesc,
            NULL,
            &error);
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
        /* Read back all the changed pieces.
         * TODO It would be best to enqueue these as async read-backs
         * as they get reported to me, and then wait on the read-backs
         * here.  But that's harder, so I'll do it like this first
         */
        size_t pieceSizeInBytes = texelSize * pieceSize.v[0] * pieceSize.v[1];
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
            region[2] = 0;

            AFK_CLCHK(clEnqueueReadImage(q, clTex, CL_TRUE, origin, region, 0, 0, &changes[s * pieceSizeInBytes], 0, NULL, NULL))
        }
    }
}

void AFK_Jigsaw::bindTexture(void)
{
    glBindBuffer(GL_TEXTURE_BUFFER, glTex);
    glBindTexture(GL_TEXTURE_BUFFER, glTex);

    if (!clGlSharing)
    {
        /* Push all the changed pieces into the GL texture. */
        size_t pieceSizeInBytes = texelSize * pieceSize.v[0] * pieceSize.v[1];
        for (unsigned int s = 0; s < changedPieces.size(); ++s)
        {
            glTexSubImage2D(
                GL_TEXTURE_BUFFER, 0,
                changedPieces[s].v[0] * pieceSize.v[0],
                changedPieces[s].v[1] * pieceSize.v[1],
                pieceSize.v[0],
                pieceSize.v[1],
                glTexFormat,
                GL_FLOAT, /* TODO will I need other? */
                &changes[s * pieceSizeInBytes]);
        }

        changedPieces.clear();
        changes.clear();
    }

    glTexBuffer(GL_TEXTURE_BUFFER, glTexFormat, glTex);
}


/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    int _pieceCount,
    GLenum _glTexFormat,
    const cl_image_format& _clTexFormat,
    size_t _texelSize,
    bool _clGlSharing):
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        glTexFormat(_glTexFormat),
        clTexFormat(_clTexFormat),
        texelSize(_texelSize),
        clGlSharing(_clGlSharing),
        box(_pieceCount)
{
    std::cout << "AFK_JigsawCollection: With " << std::dec << pieceCount << " pieces of size " << pieceSize << ": " << std::endl;

    /* TODO What I need to do here is figure out how many pieces will
     * fit into the largest jigsaw that I can make on this hardware.
     * Which I can do by experimentally making textures of type
     * GL_PROXY_TEXTURE_2D, and then requesting some simple properties
     * and checking for non-zero.
     * However, in order to verify the rest of this stuff, for now I'm
     * going to use the degenerate case and have only one piece per
     * jigsaw :P
     */
    Vec2<int> jigsawSize = afk_vec2<int>(1, 1);
    int jigsawCount = pieceCount;

    std::cout << "AFK_JigsawCollection: Making " << jigsawCount << " jigsaws with " << jigsawSize << " pieces each" << std::endl;

    /* Make a large enough buffer to fill one of the jigsaws from */
    zeroMemSize =
        pieceSize.v[0] * pieceSize.v[1] *
        jigsawSize.v[0] * jigsawSize.v[1] *
        texelSize;
    zeroMem = new unsigned char[zeroMemSize];

    /* TODO consider making one as a spare and filling the box up
     * as an async task?
     */
    for (int j = 0; j < jigsawCount; ++j)
    {
        puzzles.push_back(new AFK_Jigsaw(
            ctxt,
            pieceSize,
            jigsawSize,
            glTexFormat,
            clTexFormat,
            texelSize,
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

