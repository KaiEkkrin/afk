/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_JIGSAW_H_
#define _AFK_JIGSAW_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/lockfree/queue.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

/* This module encapsulates the idea of having a large, "heapified"
 * texture (collection of textures in fact) that I feed to the shaders
 * and access indirectly via another texture describing which offsets
 * to use.
 *
 * Whilst it might notionally be lovely to use the OpenGL mip-mapping
 * features here, that doesn't really seem to play well with an
 * infinite landscape and the juggled caching model that I'm using.
 * So for now, this will just be a flat buffer.
 */

/* This token represents which "piece" of the jigsaw an object might
 * be associated with.
 */
class AFK_JigsawPiece
{
public:
    const unsigned int piece;   /* offset within the identified jigsaw buffer */
    const unsigned int puzzle;  /* which jigsaw buffer */

    /* This constructor makes the "null" jigsaw piece, which isn't in
     * any puzzle.  Compare against this to decide if a piece has
     * been assigned or not.
     */
    AFK_JigsawPiece();

    AFK_JigsawPiece(unsigned int _piece, unsigned int _puzzle);

    bool operator==(const AFK_JigsawPiece& other) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_JigsawPiece>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_JigsawPiece>::value));


/* This encapsulates a single jigsawed buffer. */
class AFK_Jigsaw
{
protected:
    GLuint glBuf;
    cl_mem clBuf;
    unsigned int pieceSize, pieceCount;
    GLenum texFormat;
    bool clGlSharing;

    /* If clGlSharing is disabled, this is the list of pieces that
     * have been changed in the CL and need to be pushed to the GL,
     * along with the change data that I've read out (in the same
     * order).
     */
    std::vector<unsigned int> changedPieces;
    std::vector<unsigned char> changes;

public:
    AFK_Jigsaw(
        cl_context ctxt,
        unsigned int _pieceSize,
        unsigned int _pieceCount,
        GLenum _texFormat,
        bool _clGlSharing,
        unsigned char *zeroMem /* enough zeroes to initialise with glBufferData */);
    virtual ~AFK_Jigsaw();

    /* Acquires the buffer for the CL. */
    cl_mem *acquireForCl(cl_context ctxt, cl_command_queue q);

    /* Tells the jigsaw you've changed a piece (if we're not doing
     * buffer sharing, that means we will need to sync.)
     * TODO I think I'm going to want to end up supplying a list...
     */
    void pieceChanged(unsigned int piece);

    /* Releases the buffer from the CL. */
    void releaseFromCl(cl_command_queue q);

    /* Binds the buffer to the GL as a texture. */
    void bindTexture(void);
};

/* This encapsulates a collection of jigsawed buffers, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    unsigned int pieceSize, pieceCount;
    GLenum texFormat;
    bool clGlSharing;

    std::vector<AFK_Jigsaw*> puzzles;
    boost::mutex mut;
    unsigned char *zeroMem;

    /* The pieces that are free to be given out. */
    boost::lockfree::queue<AFK_JigsawPiece> box;

public:
    AFK_JigsawCollection(
        cl_context ctxt,
        unsigned int _pieceSize,
        unsigned int _pieceCount,
        GLenum _texFormat,
        unsigned int texelSize, /* Yes I could derive this from _texFormat but only with a huge switch block */
        bool _clGlSharing);
    virtual ~AFK_JigsawCollection();

    /* Gives you a piece.  This will usually be quick,
     * but it may stall if we need to add a new jigsaw
     * to the collection.
     */
    AFK_JigsawPiece grab(void);

    /* Returns a piece to the box. */
    void replace(AFK_JigsawPiece piece);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece) const;

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(unsigned int puzzle) const;
};


#endif /* _AFK_JIGSAW_H_ */

