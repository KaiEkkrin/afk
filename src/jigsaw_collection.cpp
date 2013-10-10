/* AFK (c) Alex Holloway 2013 */

#include "exception.hpp"
#include "jigsaw_collection.hpp"

#include <climits>
#include <cstring>
#include <iostream>


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

AFK_JigsawFake3DDescriptor AFK_JigsawFake3DDescriptor::operator=(
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
    if (!useFake3D) throw AFK_Exception("Not using fake 3D");
    return afk_vec3<int>(
        fakeSize.v[0] * mult,
        fakeSize.v[1] * mult,
        1);
}

Vec3<int> AFK_JigsawFake3DDescriptor::getFakeSize(void) const
{
    if (!useFake3D) throw AFK_Exception("Not using fake 3D");
    return fakeSize;
}

int AFK_JigsawFake3DDescriptor::getMult(void) const
{
    if (!useFake3D) throw AFK_Exception("Not using fake 3D");
    return mult;
}

Vec3<int> AFK_JigsawFake3DDescriptor::fake3DTo2D(const Vec3<int>& _fake) const
{
    if (!useFake3D) throw AFK_Exception("Not using fake 3D");
    return afk_vec3<int>(
        _fake.v[0] + fakeSize.v[0] * (_fake.v[2] % mult),
        _fake.v[1] + fakeSize.v[1] * (_fake.v[2] / mult),
        0);
}

Vec3<int> AFK_JigsawFake3DDescriptor::fake3DFrom2D(const Vec3<int>& _real) const
{
    if (!useFake3D) throw AFK_Exception("Not using fake 3D");
    int sFactor = (_real.v[0] / fakeSize.v[0]);
    int tFactor = (_real.v[1] / fakeSize.v[1]);
    return afk_vec3<int>(
        _real.v[0] % fakeSize.v[0],
        _real.v[1] % fakeSize.v[1],
        sFactor + mult * tFactor);
}


/* AFK_JigsawCollection implementation */

GLuint AFK_JigsawCollection::getGlTextureTarget(void) const
{
    std::ostringstream ss;

    switch (dimensions)
    {
    case AFK_JIGSAW_2D:
        return GL_TEXTURE_2D;

    case AFK_JIGSAW_3D:
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

    case AFK_JIGSAW_3D:
        ss << "3D";
        break;

    default:
        ss << "Unsupported jigsaw dimensions: " << dimensions;
        throw AFK_Exception(ss.str());
    }

    return ss.str();
}

bool AFK_JigsawCollection::grabPieceFromPuzzle(
    unsigned int threadId,
    int puzzle,
    AFK_JigsawPiece *o_piece,
    AFK_Frame *o_timestamp)
{
    Vec3<int> uvw;
    if (puzzles[puzzle]->grab(threadId, uvw, o_timestamp))
    {
        /* This check has caught a few bugs in the past... */
        assert(uvw.v[0] >= 0 && uvw.v[0] < jigsawSize.v[0] &&
            uvw.v[1] >= 0 && uvw.v[1] < jigsawSize.v[1] &&
            uvw.v[2] >= 0 && uvw.v[2] < jigsawSize.v[2]);

        *o_piece = AFK_JigsawPiece(uvw, puzzle);
        return true;
    }

    return false;
}

AFK_Jigsaw *AFK_JigsawCollection::makeNewJigsaw(cl_context ctxt) const
{
    return new AFK_Jigsaw(
        ctxt,
        pieceSize,
        jigsawSize,
        &format[0],
        dimensions == AFK_JIGSAW_2D ? GL_TEXTURE_2D : GL_TEXTURE_3D,
        texCount,
        bufferUsage,
        concurrency);
}

AFK_JigsawCollection::AFK_JigsawCollection(
    cl_context ctxt,
    const Vec3<int>& _pieceSize,
    int _pieceCount,
    int minJigsawCount,
    enum AFK_JigsawDimensions _dimensions,
    enum AFK_JigsawFormat *texFormat,
    unsigned int _texCount,
    const AFK_ClDeviceProperties& _clDeviceProps,
    enum AFK_JigsawBufferUsage _bufferUsage,
    unsigned int _concurrency,
    bool useFake3D):
        dimensions(_dimensions),
        texCount(_texCount),
        pieceSize(_pieceSize),
        pieceCount(_pieceCount),
        bufferUsage(_bufferUsage),
        concurrency(_concurrency)
{
    if (useFake3D)
    {
        fake3D = AFK_JigsawFake3DDescriptor(true, pieceSize);
        Vec3<int> realPieceSize = fake3D.get2DSize();
        std::cout << "AFK_JigsawCollection: Converting 3D piece " << pieceSize << " to 2D piece " << realPieceSize << std::endl;
        pieceSize = realPieceSize;
        dimensions = AFK_JIGSAW_2D;
    }

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
    Vec3<int> jigsawSizeIncrement = (
        dimensions == AFK_JIGSAW_2D ? afk_vec3<int>(concurrency, concurrency, 0) :
        afk_vec3<int>(concurrency, concurrency, concurrency));

    jigsawSize = (
        dimensions == AFK_JIGSAW_2D ? afk_vec3<int>(concurrency, concurrency, 1) :
        afk_vec3<int>(concurrency, concurrency, concurrency));

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
            dimensionsOK &= ((minJigsawCount * testJigsawSize.v[0] * testJigsawSize.v[1] * testJigsawSize.v[2] * pieceSize.v[0] * pieceSize.v[1] * pieceSize.v[2] * format[tex].texelSize) < (_clDeviceProps.maxMemAllocSize / 2));
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
                    nullptr);
                break;

            case AFK_JIGSAW_3D:
                glTexImage3D(
                    proxyTexTarget,
                    0,
                    format[tex].glInternalFormat,
                    pieceSize.v[0] * testJigsawSize.v[0],
                    pieceSize.v[1] * testJigsawSize.v[1],
                    pieceSize.v[2] * testJigsawSize.v[2],
					0,
                    format[tex].glFormat,
                    format[tex].glDataType,
                    nullptr);
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
        puzzles.push_back(makeNewJigsaw(ctxt));
    }

    spare = makeNewJigsaw(ctxt);

    spills.store(0);
}

AFK_JigsawCollection::~AFK_JigsawCollection()
{
    for (auto p : puzzles) delete p;
    if (spare) delete spare;
}

int AFK_JigsawCollection::getPieceCount(void) const
{
    return pieceCount;
}

void AFK_JigsawCollection::grab(
    unsigned int threadId,
    int minJigsaw,
    AFK_JigsawPiece *o_pieces,
    AFK_Frame *o_timestamps,
    size_t count)
{
    bool gotUpgradeLock = false;
    if (mut.try_lock_upgrade()) gotUpgradeLock = true;
    else mut.lock_shared();

    int puzzle;
    AFK_JigsawPiece piece;
    for (puzzle = minJigsaw; puzzle < (int)puzzles.size(); ++puzzle)
    {
        bool haveAllPieces = true;
        for (size_t pieceIdx = 0; haveAllPieces && pieceIdx < count; ++pieceIdx)
        {
            haveAllPieces &= grabPieceFromPuzzle(threadId, puzzle, &o_pieces[pieceIdx], &o_timestamps[pieceIdx]);
            
        }

        if (haveAllPieces) goto grab_return;
    }

    /* If I get here I've run out of room entirely.  See if I have
     * a spare jigsaw to push in.
     */
    if (!gotUpgradeLock)
    {
        mut.unlock_shared();
        mut.lock_upgrade();
        gotUpgradeLock = true;
    }

    mut.unlock_upgrade_and_lock();

    if (spare)
    {
        puzzles.push_back(spare);
        spare = nullptr;
    }

    if ((int)puzzles.size() == puzzle)
    {
        /* Properly out of room.  Failed. */
        mut.unlock();
        throw AFK_Exception("Jigsaw ran out of room");
    }

    mut.unlock();
    return grab(threadId, puzzle, o_pieces, o_timestamps, count);

grab_return:
    if (gotUpgradeLock) mut.unlock_upgrade();
    else mut.unlock_shared();
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(const AFK_JigsawPiece& piece)
{
    if (piece == AFK_JigsawPiece()) throw AFK_Exception("AFK_JigsawCollection: Called getPuzzle() with the null piece");
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    return puzzles[piece.puzzle];
}

AFK_Jigsaw *AFK_JigsawCollection::getPuzzle(int puzzle)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    return puzzles[puzzle];
}

int AFK_JigsawCollection::acquireAllForCl(
    cl_context ctxt,
    cl_command_queue q,
    cl_mem **allMem,
    int count,
    std::vector<cl_event>& o_events)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);

    int i;
    int puzzleCount = (int)puzzles.size();
    assert(puzzleCount <= count);

    for (i = 0; i < puzzleCount; ++i)
        allMem[i] = puzzles[i]->acquireForCl(ctxt, q, o_events);
    for (int excess = i; excess < count; ++excess)
        allMem[excess] = allMem[0];

    return i;
}

void AFK_JigsawCollection::releaseAllFromCl(
    cl_command_queue q,
    cl_mem **allMem,
    int count,
    const std::vector<cl_event>& eventWaitList)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    
    for (int i = 0; i < count; ++i)
        puzzles[i]->releaseFromCl(q, eventWaitList);
}

void AFK_JigsawCollection::flipCuboids(cl_context ctxt, const AFK_Frame& currentFrame)
{
    boost::upgrade_lock<boost::upgrade_mutex> ulock(mut);
    boost::upgrade_to_unique_lock<boost::upgrade_mutex> utoulock(ulock);

    for (int puzzle = 0; puzzle < (int)puzzles.size(); ++puzzle)
        puzzles[puzzle]->flipCuboids(currentFrame);

    if (!spare)
    {
        /* Make a new one to push along. */
        spare = makeNewJigsaw(ctxt);
    }
}

void AFK_JigsawCollection::printStats(std::ostream& os, const std::string& prefix)
{
    boost::shared_lock<boost::upgrade_mutex> lock(mut);
    std::cout << prefix << "\t: Spills:               " << spills.exchange(0) << std::endl;
    for (int puzzle = 0; puzzle < (int)puzzles.size(); ++puzzle)
    {
        std::ostringstream puzPf;
        puzPf << prefix << " " << std::dec << puzzle;
        puzzles[puzzle]->printStats(os, puzPf.str());
    }
}

