/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_JIGSAW_COLLECTION_H_
#define _AFK_JIGSAW_COLLECTION_H_

#include "afk.hpp"

#include "jigsaw.hpp"

enum AFK_JigsawDimensions
{
    AFK_JIGSAW_2D,
    AFK_JIGSAW_3D
};

/* This class describes a fake 3D image that emulates 3D with a
 * 2D image.
 * A fake 3D texture will otherwise be GL_TEXTURE_2D in the
 * Jigsaw and most Jigsaw operations will work as for 2D
 * textures.
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
    Vec3<int> fake3DTo2D(const Vec3<int>& _fake) const;
    Vec3<int> fake3DFrom2D(const Vec3<int>& _real) const;
};

/* This encapsulates a collection of jigsawed textures, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    enum AFK_JigsawDimensions dimensions;
    std::vector<AFK_JigsawFormatDescriptor> format;
    AFK_JigsawFake3DDescriptor fake3D;
    const unsigned int texCount;

    Vec3<int> pieceSize;
    Vec3<int> jigsawSize;
    int pieceCount;
    const enum AFK_JigsawBufferUsage bufferUsage;
    const unsigned int concurrency;

    std::vector<AFK_Jigsaw*> puzzles;
    AFK_Jigsaw *spare;

    boost::mutex mut;

    /* Internal helpers. */
    GLuint getGlTextureTarget(void) const;
    std::string getDimensionalityStr(void) const;
    bool grabPieceFromPuzzle(
        unsigned int threadId,
        int puzzle,
        AFK_JigsawPiece *o_piece,
        AFK_Frame *o_timestamp);

    AFK_Jigsaw *makeNewJigsaw(cl_context ctxt) const;

    /* For stats. */
    boost::atomic<unsigned long long> spills;

public:
    AFK_JigsawCollection(
        cl_context ctxt,
        const Vec3<int>& _pieceSize,
        int _pieceCount,
        int minJigsawCount,
        enum AFK_JigsawDimensions _dimensions,
        enum AFK_JigsawFormat *texFormat,
        unsigned int _texCount,
        const AFK_ClDeviceProperties& _clDeviceProps,
        enum AFK_JigsawBufferUsage _bufferUsage,
        unsigned int concurrency,
        bool useFake3D);
    virtual ~AFK_JigsawCollection();

    int getPieceCount(void) const;

    /* Gives you a some pieces.  This will usually be quick,
     * but it may stall if we need to add a new jigsaw
     * to the collection.
     * The pieces are guaranteed to all come from the same
     * jigsaw.
     * Also fills out `o_timestamp' with the timestamp of the row
     * your piece came from so you can find out when it's
     * going to be swept.
     * `minJigsaw' tells it which index jigsaw to try grabbing
     * from.  This is an attempt at a cunning trick by which I
     * can separate long-lived pieces (to start at jigsaw 0)
     * from shorter-lived pieces (to start at a higher jigsaw)
     * to avoid repeatedly sweeping out long-lived pieces only
     * to re-create them the same.
     */
    void grab(
        unsigned int threadId,
        int minJigsaw,
        AFK_JigsawPiece *o_pieces,
        AFK_Frame *o_timestamps,
        size_t count);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece) const;

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle) const;

    /* Gets you the puzzle count. */
    int getPuzzleCount(void) const;

    /* Flips the cuboids in all the jigsaws. */
    void flipCuboids(cl_context ctxt, const AFK_Frame& currentFrame);

    void printStats(std::ostream& os, const std::string& prefix);
};

#endif /* _AFK_JIGSAW_COLLECTION_H_ */

