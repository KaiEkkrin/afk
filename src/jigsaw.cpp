/* AFK (c) Alex Holloway 2013 */

#include "display.hpp"
#include "exception.hpp"
#include "jigsaw.hpp"

#include <cstring>


/* AFK_JigsawPiece implementation */

AFK_JigsawPiece::AFK_JigsawPiece():
    piece(0xffffffffu), puzzle(0xffffffffu)
{
}

AFK_JigsawPiece::AFK_JigsawPiece(unsigned int _piece, unsigned int _puzzle):
    piece(_piece), puzzle(_puzzle)
{
}

bool AFK_JigsawPiece::operator==(const AFK_JigsawPiece& other) const
{
    return (piece == other.piece && puzzle == other.puzzle);
}

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece)
{
    return os << "JigsawPiece(piece=" << std::dec << piece.piece << ", puzzle=" << piece.puzzle << ")";
}


/* AFK_Jigsaw implementation */
AFK_Jigsaw::AFK_Jigsaw(
    cl_context ctxt,
    unsigned int _pieceSize,
    unsigned int _pieceCount,
    GLenum _texFormat,
    bool _clGlSharing,
    unsigned char *zeroMem):
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        texFormat(_texFormat),
        clGlSharing(_clGlSharing)
{
    glGenBuffers(1, &glBuf);
    glBindBuffer(GL_TEXTURE_BUFFER, glBuf);
    glBufferData(GL_TEXTURE_BUFFER, pieceSize * pieceCount, zeroMem, GL_DYNAMIC_DRAW);
    AFK_GLCHK("AFK_JigSaw bufferData")

    cl_int error;
    if (clGlSharing)
    {
        clBuf = clCreateFromGLBuffer(
            ctxt, CL_MEM_READ_WRITE, glBuf, &error);
    }
    else
    {
        /* I don't bother initialising this.  It's supposed to be
         * initialised on first compute anyway -- the only reason
         * I have the zeroes is because OpenGL sizes buffers like
         * that.
         */
        clBuf = clCreateBuffer(
            ctxt, CL_MEM_READ_WRITE, pieceSize * pieceCount, NULL, &error);
    }
    afk_handleClError(error);
}

AFK_Jigsaw::~AFK_Jigsaw()
{
    clReleaseMemObject(clBuf);
    glDeleteBuffers(1, &glBuf);
}

cl_mem *AFK_Jigsaw::acquireForCl(cl_context ctxt, cl_command_queue q)
{
    if (clGlSharing)
    {
        AFK_CLCHK(clEnqueueAcquireGLObjects(q, 1, &clBuf, 0, 0, 0))
    }
    return &clBuf;
}

void AFK_Jigsaw::pieceChanged(unsigned int piece)
{
    if (!clGlSharing) changedPieces.push_back(piece);
}

void AFK_Jigsaw::releaseFromCl(cl_command_queue q)
{
    if (clGlSharing)
    {
        AFK_CLCHK(clEnqueueReleaseGLObjects(q, 1, &clBuf, 0, 0, 0))
    }
    else
    {
        /* Read back all the changed pieces.
         * TODO It would be best to enqueue these as async read-backs
         * as they get reported to me, and then wait on the read-backs
         * here.  But that's harder, so I'll do it like this first
         */
        if (changes.size() < (changedPieces.size() * pieceSize))
            changes.resize(changedPieces.size() * pieceSize);

        for (unsigned int s = 0; s < changedPieces.size(); ++s)
        {
            AFK_CLCHK(clEnqueueReadBuffer(q, clBuf, CL_TRUE, changedPieces[s] * pieceSize, pieceSize, &changes[s * pieceSize], 0, NULL, NULL))
        }
    }
}

void AFK_Jigsaw::bindTexture(void)
{
    glBindBuffer(GL_TEXTURE_BUFFER, glBuf);
    glBindTexture(GL_TEXTURE_BUFFER, glBuf);
    glTexBuffer(GL_TEXTURE_BUFFER, texFormat, glBuf);
}


/* AFK_JigsawCollection implementation */

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    unsigned int _pieceSize,
    unsigned int _pieceCount,
    GLenum _texFormat,
    unsigned int texelSize,
    bool _clGlSharing):
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        texFormat(_texFormat),
        clGlSharing(_clGlSharing),
        box(_pieceCount)
{
    std::cout << "AFK_JigsawCollection: With " << std::dec << pieceCount << " pieces of size " << pieceSize << ": " << std::endl;

    /* Work out the largest possible jigsaw...
     * TODO Am I going to need to involve some of the
     * CL parameters in this calculation, too?
     */
    GLint maxTextureBufferSize;
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxTextureBufferSize);

    unsigned int texelCount = pieceSize * pieceCount / texelSize;
    unsigned int jigsawCount = 1;

    unsigned int texelCountPerJigsaw;
    for (texelCountPerJigsaw = texelCount;
        texelCountPerJigsaw > maxTextureBufferSize;
        texelCountPerJigsaw = texelCountPerJigsaw / 2, jigsawCount = jigsawCount * 2);

    unsigned int pieceCountPerJigsaw = texelCountPerJigsaw * texelSize / pieceSize;
    std::cout << "AFK_JigsawCollection: Making " << jigsawCount << " jigsaws with " << pieceCountPerJigsaw << " pieces each" << std::endl;

    /* TODO consider making one as a spare and filling the box up
     * as an async task?
     */
    zeroMem = new unsigned char[pieceCountPerJigsaw * pieceSize];
    memset(zeroMem, 0, pieceCountPerJigsaw * pieceSize);

    for (unsigned int j = 0; j < jigsawCount; ++j)
    {
        puzzles.push_back(new AFK_Jigsaw(
            ctxt,
            pieceSize,
            pieceCountPerJigsaw,
            texFormat,
            clGlSharing,
            zeroMem));

        for (unsigned int p = 0; p < pieceCountPerJigsaw; ++p)
            box.push(AFK_JigsawPiece(p, j));
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

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(unsigned int puzzle) const
{
    return puzzles[puzzle];
}

