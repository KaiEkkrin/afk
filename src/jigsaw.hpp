/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_JIGSAW_H_
#define _AFK_JIGSAW_H_

#include "afk.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

#include <boost/function.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "computer.hpp"
#include "data/moving_average.hpp"
#include "def.hpp"

/* This module encapsulates the idea of having a large, "heapified"
 * texture (collection of textures in fact) that I feed to the shaders
 * and access indirectly via another texture describing which offsets
 * to use.
 *
 * Whilst it might notionally be lovely to use the OpenGL mip-mapping
 * features here, that doesn't really seem to play well with an
 * infinite landscape and the juggled caching model that I'm using.
 * So for now, this will be a flat 2D texture with no mip levels.
 */

/* This enumeration describes the jigsaw's texture format.  I'll
 * need to add more here as I support more formats.
 */
enum AFK_JigsawFormat
{
    AFK_JIGSAW_FLOAT32,
    AFK_JIGSAW_555A1,
    AFK_JIGSAW_101010A2,
    AFK_JIGSAW_4FLOAT8_UNORM,
    AFK_JIGSAW_4FLOAT8_SNORM,
    AFK_JIGSAW_4HALF32,
    AFK_JIGSAW_4FLOAT32
};

class AFK_JigsawFormatDescriptor
{
public:
    GLint glInternalFormat;
    GLenum glFormat;
    GLenum glDataType;
    cl_image_format clFormat;
    size_t texelSize;

    AFK_JigsawFormatDescriptor(enum AFK_JigsawFormat);
    AFK_JigsawFormatDescriptor(const AFK_JigsawFormatDescriptor& _fd);
};

/* This token represents which "piece" of the jigsaw an object might
 * be associated with.
 */
class AFK_JigsawPiece
{
public:
    Vec2<int> piece;   /* u, v of the jigsaw piece within the jigsaw, in piece units */
    int puzzle;  /* which jigsaw buffer */

    /* This constructor makes the "null" jigsaw piece, which isn't in
     * any puzzle.  Compare against this to decide if a piece has
     * been assigned or not.
     */
    AFK_JigsawPiece();

    AFK_JigsawPiece(const Vec2<int>& _piece, int _puzzle);

    bool operator==(const AFK_JigsawPiece& other) const;
    bool operator!=(const AFK_JigsawPiece& other) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_JigsawPiece>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_JigsawPiece>::value));


/* This abstract class encapsulates the metadata associated with a
 * jigsaw piece.  The code that interfaces with the Jigsaw needs to
 * supply appropriate objects.
 */
class AFK_JigsawMetadata
{
public:
    virtual void assigned(const Vec2<int>& uv) = 0;
    virtual bool canEvict(const AFK_Frame& frame) = 0;

    /* All pieces with u=row are evicted. */
    virtual void evicted(const int row) = 0;
};


/* This class encapsulates a sub-rectangle within the jigsaw that
 * contains all the updates from a frame (we write to it) or all
 * the draws (we use it to sort out rectangular reads and writes
 * of the texture itself).
 */
class AFK_JigsawSubRect
{
public:
    /* These co-ordinates are in piece units. */
    int x, y, rows, columns;

    boost::mutex mut;

    AFK_JigsawSubRect();
};


/* This encapsulates a single jigsawed texture, which may contain
 * multiple textures, one for each format in the list: they will all
 * have identical layouts and be referenced with the same jigsaw
 * pieces.
 */
class AFK_Jigsaw
{
protected:
    GLuint *glTex;
    cl_mem *clTex;
    const AFK_JigsawFormatDescriptor *format;
    const unsigned int texCount;

    const Vec2<int> pieceSize;
    const Vec2<int> jigsawSize; /* number of pieces horizontally and vertically */
    const bool clGlSharing;

    /* Each row of the jigsaw is associated with the frame it was
     * last seen, so that I can decide whether I can evict a row.
     */
    std::vector<AFK_Frame> rowLastSeen;

    /* We fill each jigsaw row left to right, and only ever clear
     * an entire row at a time.  This stops me from having to track
     * the occupancy of individual pieces.  This list associates
     * each row of the jigsaw with the number of pieces in it that
     * are occupied, starting from the left.
     * In order to make neat rectangles, the flipRects() function
     * needs to add pretend usage to all short rows that were in the
     * most recent update's rectangle, of course...
     */
    std::vector<int> rowUsage;

    /* Here's the jigsaw metadata. */
    boost::shared_ptr<AFK_JigsawMetadata> meta;

    /* Each frame, we assign new pieces out of sub-rectangles of
     * the jigsaw.  We try to make it `concurrency' rows high by
     * as many columns wide as is necessary.  (Hopefully the multi-
     * threading model will cause the rows to be of similar widths.)
     * These are the current updating and drawing rectangles in piece
     * units.
     * These things are lists, just in case I need to start a new
     * sub-rectangle at any point.
     */
    std::vector<AFK_JigsawSubRect> rects[2];
    unsigned int updateRect;
    unsigned int drawRect;

    /* If clGlSharing is disabled, this is the event I need to wait on
     * before I can push data to the GL.
     */
    cl_event changeEvent;

    /* This utility function returns the actual jigsaw row at a
     * given rectangle x origin and offset (including wrapping).
     */
    int wrapRows(int origin, int offset) const;

public:
    AFK_Jigsaw(
        cl_context ctxt,
        const Vec2<int>& _pieceSize,
        const Vec2<int>& _jigsawSize,
        const AFK_JigsawFormatDescriptor *_format,
        unsigned int _texCount,
        bool _clGlSharing,
        boost::shared_ptr<AFK_JigsawMetadata> _meta);
    virtual ~AFK_Jigsaw();

    /* Acquires a new piece for your thread
     * If this function returns false, this jigsaw has run out of
     * space and the JigsawCollection needs to use a different one.
     * This function should be thread safe so long as you don't
     * lie about your thread ID
     * TODO: The JigsawCollection ought to cache this function's
     * returning false each frame, so that it doesn't send threads
     * back to the wrong jigsaw after it ran out of space.
     */
    bool grab(unsigned int threadId, Vec2<int>& o_uv);

    unsigned int getTexCount(void) const;

    /* Returns the (s, t) texture co-ordinates for a given piece
     * within the jigsaw.  These will be in the range (0, 1).
     */
    Vec2<float> getTexCoordST(const AFK_JigsawPiece& piece) const;

    /* Returns the (s, t) dimensions of one piece within the jigsaw. */
    Vec2<float> getPiecePitchST(void) const;

    /* Acquires the buffers for the CL. */
    cl_mem *acquireForCl(cl_context ctxt, cl_command_queue q);

    /* Releases the buffer from the CL. */
    void releaseFromCl(cl_command_queue q);

    /* Binds a buffer to the GL as a texture.
     * Call once for each texture, having set glActiveTexture()
     * appropriately each time.
     */
    void bindTexture(unsigned int tex);

    /* Signals to the jigsaw to flip the rectangles over and start off
     * a new update rectangle.
     */
    void flipRects(void);
};

/* This encapsulates a collection of jigsawed textures, which are used
 * to give out pieces of the same size and usage.
 * You may get a piece in any of the puzzles.
 */
class AFK_JigsawCollection
{
protected:
    std::vector<AFK_JigsawFormatDescriptor> format;
    const unsigned int texCount;

    const Vec2<int> pieceSize;
    Vec2<int> jigsawSize;
    int pieceCount;
    const bool clGlSharing;

    std::vector<AFK_Jigsaw*> puzzles;
    boost::mutex mut;

    /* How to make a new metadata object for a new puzzle. */
    boost::function<boost::shared_ptr<AFK_JigsawMetadata> (void)> newMeta;

public:
    AFK_JigsawCollection(
        cl_context ctxt,
        const Vec2<int>& _pieceSize,
        int _pieceCount,
        enum AFK_JigsawFormat *texFormat,
        unsigned int _texCount,
        const AFK_ClDeviceProperties& _clDeviceProps,
        bool _clGlSharing,
        boost::function<boost::shared_ptr<AFK_JigsawMetadata> (void)> _newMeta);
    virtual ~AFK_JigsawCollection();

    /* Gives you a piece.  This will usually be quick,
     * but it may stall if we need to add a new jigsaw
     * to the collection.
     */
    AFK_JigsawPiece grab(unsigned int threadId);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece) const;

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle) const;
};


#endif /* _AFK_JIGSAW_H_ */

