/* AFK (c) Alex Holloway 2013 */

#include "exception.hpp"
#include "jigsaw_collection.hpp"

#include <climits>
#include <cstring>
#include <iostream>


/* AFK_JigsawCollection implementation */

GLuint AFK_JigsawCollection::getGlTextureTarget(void) const
{
    std::ostringstream ss;

    switch (dimensions)
    {
    case AFK_JIGSAW_2D:
        return GL_TEXTURE_2D;

    case AFK_JIGSAW_3D;
        return GL_TEXTURE_3D;

    default:
        ss << "Unsupported jigsaw dimensions: " << dimensions;
        throw AFK_Exception(ss.str());
    }
}

std::string AFK_JigsawCollection::getDimensionalityStr(void) const
{
    std::ostringstream ss;

    switch (dimensions)
    {
    case AFK_JIGSAW_2D:
        ss << "2D";
        break;

    case AFK_JIGSAW_3D;
        ss << "3D";
        break;

    default:
        ss << "Unsupported jigsaw dimensions: " << dimensions;
        throw AFK_Exception(ss.str());
    }

    return ss.str();
}

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    const Vec2<int>& _pieceSize,
    int _pieceCount,
    int minJigsawCount,
    enum AFK_JigsawDimensions _dimensions,
    enum AFK_JigsawFormat *texFormat,
    unsigned int _texCount,
    const AFK_ClDeviceProperties& _clDeviceProps,
    bool _clGlSharing,
    unsigned int _concurrency):
        dimensions(_dimensions),
        texCount(_texCount),
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        clGlSharing(_clGlSharing),
        concurrency(_concurrency)
{
    std::cout << "AFK_JigsawCollection: Requested " << getDimensionalityStr() << " jigsaw with " << std::dec << pieceCount << " pieces of size " << pieceSize << ": " << std::endl;

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
    Vec2<int> jigsawSizeIncrement = (
        dimensions == AFK_JIGSAW_2D ? afk_vec2<int>(concurrency, concurrency, 0) :
        afk_vec2<int>(concurrency, concurrency, concurrency));

    jigsawSize = (
        dimensions == AFK_JIGSAW_2D ? afk_vec2<int>(concurrency, concurrency, 1) :
        afk_vec2<int>(concurrency, concurrency, concurrency));

    GLuint proxyTexTarget = (dimensions == AFK_JIGSAW_2D ? GL_PROXY_TEXTURE_2D : GL_PROXY_TEXTURE_3D);
    GLuint glProxyTex[texCount];
    glGenTextures(texCount, glProxyTex);
    GLint texWidth;
    bool dimensionsOK = true;
    for (Vec3<int> testJigsawSize = jigsawSize;
        dimensionsOK && jigsawSize.v[0] * jigsawSize.v[1] < pieceCount;
        testJigsawSize += jigsawSizeIncrement)
    {
        dimensionsOK = (
            dimensions == AFK_JIGSAW_2D ?
                (testJigsawSize.v[0] <= (int)_clDeviceProps.image2DMaxWidth &&
                 testJigsawSize.v[1] <= (int)_clDeviceProps.image2DMaxHeight) :
                (testJigsawSize.v[0] <= (int)_clDeviceProps.image3DMaxWidth &&
                 testJigsawSize.v[1] <= (int)_clDeviceProps.image3DMaxHeight &&
                 testJigsawSize.v[2] <= (int)_clDeviceProps.image3DMaxDepth));

        /* Try to make pretend textures of the current jigsaw size */
        for (unsigned int tex = 0; tex < texCount && dimensionsOK; ++tex)
        {
            dimensionsOK &= ((minJigsawCount * testJigsawSize.v[0] * testJigsawSize.v[1] * testJigsawSize.v[2] * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2] * format[tex].texelSize) < _clDeviceProps.maxMemAllocSize);
            if (!dimensionsOK) break;

            glBindTexture(proxyTexTarget, glProxyTex[tex]);
            switch (dimensions)
            {
            case AFK_JIGSAW_2D:
                glTexImage2D(
                    proxyTexTarget,
                    0,
                    format[tex].glInternalFormat,
                    pieceSize.v[0] * testJigsawSize.v[0],
                    pieceSize.v[1] * testJigsawSize.v[1],
                    0,
                    format[tex].glFormat,
                    format[tex].glDataType,
                    NULL);
                break;

            case AFK_JIGSAW_3D:
                glTexImage3D(
                    proxyTexTarget,
                    0,
                    format[tex].glInternalFormat,
                    pieceSize.v[0] * testJigsawSize.v[0],
                    pieceSize.v[1] * testJigsawSize.v[1],
                    pieceSize.v[2] * testJigsawSize.v[2],
                    format[tex].glFormat,
                    format[tex].glDataType,
                    NULL);
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
        }

        if (dimensionsOK) jigsawSize = testJigsawSize;
    }

    glDeleteTextures(texCount, glProxyTex);
    glGetError(); /* Throw away any error that might have popped up */

    /* Update the dimensions and actual piece count to reflect what I found */
    int jigsawCount = pieceCount / (jigsawSize.v[0] * jigsawSize.v[1] * jigsawSize.v[2]) + 1;
    if (jigsawCount < minJigsawCount) jigsawCount = minJigsawCount;
    pieceCount = jigsawCount * jigsawSize.v[0] * jigsawSize.v[1] * jigsawSize.v[2];

    std::cout << "AFK_JigsawCollection: Making " << jigsawCount << " jigsaws with " << jigsawSize << " pieces each (actually " << pieceCount << " pieces)" << std::endl;

    for (int j = 0; j < jigsawCount; ++j)
    {
        puzzles.push_back(new AFK_Jigsaw(
            ctxt,
            pieceSize,
            jigsawSize,
            &format[0],
            dimensions == AFK_JIGSAW_2D ? GL_TEXTURE_2D : GL_TEXTURE_3D,
            texCount,
            clGlSharing,
            concurrency));
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

AFK_JigsawPiece AFK_JigsawCollection::grab(unsigned int threadId, int minJigsaw, AFK_Frame& o_timestamp)
{
    Vec3<int> uvw;

    int puzzle;
    for (puzzle = minJigsaw; puzzle < (int)puzzles.size(); ++puzzle)
    {
        if (puzzles[puzzle]->grab(threadId, uvw, o_timestamp))
        {
            /* TODO Paranoia */
            if (uvw.v[0] < 0 || uvw.v[0] >= jigsawSize.v[0] ||
                uvw.v[1] < 0 || uvw.v[1] >= jigsawSize.v[1] ||
                uvw.v[2] < 0 || uvw.v[2] >= jigsawSize.v[2])
            {
                std::ostringstream ss;
                ss << "Got erroneous piece: " << uvw << " (jigsaw size: " << jigsawSize << ")";
                throw AFK_Exception(ss.str());
            }

            return AFK_JigsawPiece(uvw, puzzle);
        }
    }

    /* If I get here, I've failed, because I shouldn't be going along
     * spawning extra jigsaws :P
     */
    throw AFK_Exception("Jigsaw ran out of room");
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

void AFK_JigsawCollection::flipCuboids(cl_context ctxt, const AFK_Frame& currentFrame)
{
    for (int puzzle = 0; puzzle < (int)puzzles.size(); ++puzzle)
        puzzles[puzzle]->flipCuboids(currentFrame);
}


