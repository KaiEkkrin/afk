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

/* This encapsulates a collection of jigsawed textures, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    enum AFK_JigsawDimensions dimensions;
    std::vector<AFK_JigsawFormatDescriptor> format;
    const unsigned int texCount;

    const Vec3<int> pieceSize;
    Vec3<int> jigsawSize;
    int pieceCount;
    const bool clGlSharing;
    const unsigned int concurrency;

    std::vector<AFK_Jigsaw*> puzzles;

    boost::mutex mut;

    /* Internal helpers. */
            if (dimensions == AFK_JIGSAW_2D)
            {
    GLuint getGlTextureTarget(void) const;
    std::string getDimensionalityStr(void) const;

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
        bool _clGlSharing,
        unsigned int concurrency);
    virtual ~AFK_JigsawCollection();

    int getPieceCount(void) const;

    /* Gives you a piece.  This will usually be quick,
     * but it may stall if we need to add a new jigsaw
     * to the collection.
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
    AFK_JigsawPiece grab(unsigned int threadId, int minJigsaw, AFK_Frame& o_timestamp);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece) const;

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle) const;

    /* Flips the cuboids in all the jigsaws. */
    void flipCuboids(cl_context ctxt, const AFK_Frame& currentFrame);
};

#endif /* _AFK_JIGSAW_COLLECTION_H_ */

