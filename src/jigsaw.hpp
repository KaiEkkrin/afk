/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_JIGSAW_H_
#define _AFK_JIGSAW_H_

#include "afk.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/function.hpp>
#include <boost/lockfree/queue.hpp>
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
    AFK_JIGSAW_4HALF16,
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


/* This class encapsulates a sub-rectangle within the jigsaw that
 * contains all the updates from a frame (we write to it) or all
 * the draws (we use it to sort out rectangular reads and writes
 * of the texture itself).
 */
class AFK_JigsawSubRect
{
public:
    /* These co-ordinates are in piece units.
     * During usage, a rectangle can't grow in rows, but it
     * can grow in columns.
     */
    int r, c, rows;
    boost::atomic<int> columns;

    AFK_JigsawSubRect(int _r, int _c, int _rows);

    AFK_JigsawSubRect(const AFK_JigsawSubRect& other);
    AFK_JigsawSubRect operator=(const AFK_JigsawSubRect& other);

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawSubRect& sr);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawSubRect& sr);

/* This is an internal thing for indicating whether we grabbed a
 * rectangle piece.
 */
enum AFK_JigsawPieceGrabStatus
{
    AFK_JIGSAW_RECT_OUT_OF_COLUMNS,
    AFK_JIGSAW_RECT_OUT_OF_ROWS,
    AFK_JIGSAW_RECT_GRABBED
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

    /* Each row of the jigsaw has a timestamp.  When the sweep
     * comes around and updates it, that indicates that any old
     * tiles holding pieces with an old timestamp should drop them
     * and re-generate their tiles: the row is about to be re-used.
     * (If I don't do forced eviction from the jigsaw like this,
     * I'll end up with jigsaw fragmentation problems.)
     */
    std::vector<AFK_Frame> rowTimestamp;

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

    /* Each frame, we assign new pieces out of sub-rectangles of
     * the jigsaw.  We try to make it `concurrency' rows high by
     * as many columns wide as is necessary.  (Hopefully the multi-
     * threading model will cause the rows to be of similar widths.)
     * These are the current updating and drawing rectangles in piece
     * units.
     * These things are lists, just in case I need to start a new
     * sub-rectangle at any point.
     */
    const unsigned int concurrency;
    std::vector<AFK_JigsawSubRect> rects[2];
    unsigned int updateRs;
    unsigned int drawRs;

    /* This is the sweep position, which tracks ahead of the update
     * rectangles.  Every flip, we should move the sweep position up
     * and re-timestamp all the rows it passes, telling the enumerators
     * to-regenerate the pieces of any tiles that still have pieces
     * within the sweep.
     */
    int sweepRow;

    /* This is used to control access to the update rectangles. */
    boost::mutex updateRMut;

    /* This is the average number of columns that rectangles seem to
     * be using.  It's used as a heuristic to decide whether to start
     * rectangles on new rows or not.
     * Update it in flipRects().
     */
    AFK_MovingAverage<int> columnCounts;

    /* If clGlSharing is disabled, this is the rectangle data I've
     * read back from the CL and that needs to go into the GL.
     * There is one vector per texture.
     */
    std::vector<unsigned char> *changeData;

    /* If clGlSharing is disabled, these are the events I need to
     * wait on before I can push data to the GL.
     * Again, there is one vector per texture.
     */
    std::vector<cl_event> *changeEvents;

    /* This utility function attempts to assign a piece out of the
     * current rectangle.
     * It returns:
     * - AFK_JIGSAW_RECT_OUT_OF_COLUMNS if it ran out of columns
     * (you should start a new rectangle)
     * - AFK_JIGSAW_RECT_OUT_OF_ROWS if it ran out of rows to use
     * (you should give up and tell the collection to use the next
     * jigsaw)
     * - AFK_JIGSAW_RECT_GRABBED if it succeeded.
     */
    enum AFK_JigsawPieceGrabStatus grabPieceFromRect(
        unsigned int rect,
        unsigned int threadId,
        Vec2<int>& o_uv,
        AFK_Frame& o_timestamp);

    /* Helper for the below -- having picked a new rectangle,
     * pushes it onto the update list and sets the row usage
     * properly.
     */
    void pushNewRect(AFK_JigsawSubRect rect);

    /* This utility function attempts to start a new rectangle,
     * pushing it back onto rects[updateRs], evicting anything
     * if required.
     * Set `startNewRow' to false if you'd like it to try to
     * continue the row that has the last rectangle on it.  You
     * should probably only be setting that if you're the flipRects()
     * call trying to pack the texture nicely, rather than if you're
     * a grab() call that's just run out of room!
     * May return fewer rows than `rowsRequired' if it hits the top.
     * Only actually returns an error if it can't get any rows at all.
     * Returns true on success, else false.
     */
    bool startNewRect(const AFK_JigsawSubRect& lastRect, bool startNewRow);

    int roundUpToConcurrency(int r) const;
    int getSweepTarget(int latestRow) const;

    /* Actually performs the sweep up to the given target row.
     */
    void sweep(int sweepTarget, const AFK_Frame& currentFrame);

    /* This utility function sweeps in front of the given next
     * free row, hoping to keep far enough ahead of the
     * grabber to not be caught up on.
     */
    void doSweep(int nextFreeRow, const AFK_Frame& currentFrame);

public:
    AFK_Jigsaw(
        cl_context ctxt,
        const Vec2<int>& _pieceSize,
        const Vec2<int>& _jigsawSize,
        const AFK_JigsawFormatDescriptor *_format,
        unsigned int _texCount,
        bool _clGlSharing,
        unsigned int _concurrency);
    virtual ~AFK_Jigsaw();

    /* Acquires a new piece for your thread.
     * If this function returns false, this jigsaw has run out of
     * space and the JigsawCollection needs to use a different one.
     * This function should be thread safe so long as you don't
     * lie about your thread ID
     * TODO: The JigsawCollection ought to cache this function's
     * returning false each frame, so that it doesn't send threads
     * back to the wrong jigsaw after it ran out of space.
     */
    bool grab(unsigned int threadId, Vec2<int>& o_uv, AFK_Frame& o_timestamp);

    /* Returns the time your piece was last swept.
     * If you retrieved it at that time or later, you're OK and you
     * can hang on to it.
     */
    AFK_Frame getTimestamp(const Vec2<int>& uv) const;

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
    void flipRects(const AFK_Frame& currentFrame);
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
    const unsigned int concurrency;

    std::vector<AFK_Jigsaw*> puzzles;

    boost::mutex mut;

public:
    AFK_JigsawCollection(
        cl_context ctxt,
        const Vec2<int>& _pieceSize,
        int _pieceCount,
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
     */
    AFK_JigsawPiece grab(unsigned int threadId, AFK_Frame& o_timestamp);

    /* Gets you the puzzle that matches a particular piece. */
    AFK_Jigsaw *getPuzzle(const AFK_JigsawPiece& piece) const;

    /* Gets you a numbered puzzle. */
    AFK_Jigsaw *getPuzzle(int puzzle) const;

    /* Flips the rectangles in all the jigsaws. */
    void flipRects(cl_context ctxt, const AFK_Frame& currentFrame);
};


#endif /* _AFK_JIGSAW_H_ */

