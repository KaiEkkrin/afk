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
#include "data/frame.hpp"
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
 * So for now, this will be a flat texture with no mip levels.
 */

/* In order to support both 2D and 3D textures, I'm changing Jigsaw so
 * that it is inherently 3D.  2D textures will be supported by setting
 * the `w' co-ordinate (`slice') to 0 at all times, and having a 3rd
 * jigsaw size dimension of 1.
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
    /* Co-ordinates within the jigsaw, in piece units */
    int u;
    int v;
    int w;

    /* Which jigsaw puzzle. */
    int puzzle;

    /* This constructor makes the "null" jigsaw piece, which isn't in
     * any puzzle.  Compare against this to decide if a piece has
     * been assigned or not.
     */
    AFK_JigsawPiece();

    AFK_JigsawPiece(int _u, int _v, int _w, int _puzzle);
    AFK_JigsawPiece(const Vec3<int>& _piece, int _puzzle);

    bool operator==(const AFK_JigsawPiece& other) const;
    bool operator!=(const AFK_JigsawPiece& other) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_JigsawPiece>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_JigsawPiece>::value));


/* This class encapsulates a cuboid within the jigsaw that
 * contains all the updates from a frame (we write to it) or all
 * the draws (we use it to sort out rectangular reads and writes
 * of the texture itself).
 */
class AFK_JigsawCuboid
{
public:
    /* These co-ordinates are in piece units.
     * During usage, a cuboid can't grow in rows or slices, but it
     * can grow in columns.
     */
    int r, c, s, rows, slices;
    boost::atomic<int> columns;

    AFK_JigsawCuboid(int _r, int _c, int _s, int _rows, int _slices);

    AFK_JigsawCuboid(const AFK_JigsawCuboid& other);
	AFK_JigsawCuboid operator=(const AFK_JigsawCuboid& other);

    friend std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& sr);
};

std::ostream& operator<<(std::ostream& os, const AFK_JigsawCuboid& sr);

/* This is an internal thing for indicating whether we grabbed a
 * cuboid.
 */
enum AFK_JigsawPieceGrabStatus
{
    AFK_JIGSAW_CUBOID_OUT_OF_COLUMNS,
    AFK_JIGSAW_CUBOID_OUT_OF_SPACE,
    AFK_JIGSAW_CUBOID_GRABBED
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
    GLuint texTarget;
    const unsigned int texCount;

    const Vec3<int> pieceSize;
    const Vec3<int> jigsawSize; /* number of pieces in each dimension */
    const bool clGlSharing;

    /* Each row of the jigsaw has a timestamp.  When the sweep
     * comes around and updates it, that indicates that any old
     * tiles holding pieces with an old timestamp should drop them
     * and re-generate their tiles: the row is about to be re-used.
     * (If I don't do forced eviction from the jigsaw like this,
     * I'll end up with jigsaw fragmentation problems.)
     */
    AFK_Frame **rowTimestamp;

    /* We fill each jigsaw row left to right, and only ever clear
     * an entire row at a time.  This stops me from having to track
     * the occupancy of individual pieces.  This list associates
     * each row of the jigsaw with the number of pieces in it that
     * are occupied, starting from the left.
     * In order to make neat cuboids, the flipCuboids() function
     * needs to add pretend usage to all short rows that were in the
     * most recent update's cuboid, of course...
     * Anyway, this is (row, slice).
     */
    int **rowUsage;

    /* Each frame, we assign new pieces out of cuboids cut from
     * the jigsaw.  We try to make it `concurrency' rows high and deep
     * (or just `concurrency' high by 1 deep for a 2D jigsaw) by
     * as many columns wide as is necessary.  (Hopefully the multi-
     * threading model will cause the rows to be of similar widths.)
     * These are the current updating and drawing cuboids in piece
     * units.
     */
    const unsigned int concurrency;
    std::vector<AFK_JigsawCuboid> cuboids[2];
    unsigned int updateCs;
    unsigned int drawCs;

    /* This is the sweep position, which tracks ahead of the update
     * cuboids.  Every flip, we should move the sweep position up and back
     * and re-timestamp all the rows it passes, telling the enumerators
     * to-regenerate the pieces of any tiles that still have pieces
     * within the sweep.
     */
    Vec2<int> sweepPosition;

    /* This is used to control access to the update cuboids. */
    boost::mutex updateCMut;

    /* This is the average number of columns that cuboids seem to
     * be using.  It's used as a heuristic to decide whether to start
     * cuboids on new rows or not.
     * Update it in flipCuboids().
     */
    AFK_MovingAverage<int> columnCounts;

    /* If clGlSharing is disabled, this is the cuboid data I've
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
     * current cuboid.
     * It returns:
     * - AFK_JIGSAW_CUBOID_OUT_OF_COLUMNS if it ran out of columns
     * (you should start a new cuboid)
     * - AFK_JIGSAW_CUBOID_OUT_OF_SPACE if it ran out of rows to use
     * (you should give up and tell the collection to use the next
     * jigsaw)
     * - AFK_JIGSAW_CUBOID_GRABBED if it succeeded.
     */
    enum AFK_JigsawPieceGrabStatus grabPieceFromCuboid(
        AFK_JigsawCuboid& cuboid,
        unsigned int threadId,
        Vec3<int>& o_uvw,
        AFK_Frame& o_timestamp);

    /* Helper for the below -- having picked a new cuboid,
     * pushes it onto the update list and sets the row usage
     * properly.
     */
    void pushNewCuboid(AFK_JigsawCuboid cuboid);

    /* This utility function attempts to start a new cuboid,
     * pushing it back onto cuboids[updateCs], evicting anything
     * if required.
     * Set `startNewRow' to false if you'd like it to try to
     * continue the row that has the last cuboid on it.  You
     * should probably only be setting that if you're the flipCuboids()
     * call trying to pack the texture nicely, rather than if you're
     * a grab() call that's just run out of room!
     * May return fewer rows than `rowsRequired' if it hits the top.
     * Only actually returns an error if it can't get any rows at all.
     * Returns true on success, else false.
     */
    bool startNewCuboid(const AFK_JigsawCuboid& lastCuboid, bool startNewRow);

    int roundUpToConcurrency(int r) const;
    Vec2<int> getSweepTarget(const Vec2<int>& latest) const;

    /* Helper for the below -- sweeps one slice. */
    void sweepOneSlice(int sweepRowTarget, const AFK_Frame& currentFrame);

    /* Actually performs the sweep up to the given target row.
     */
    void sweep(const Vec2<int>& sweepTarget, const AFK_Frame& currentFrame);

    /* This utility function sweeps in front of the given next
     * free row, hoping to keep far enough ahead of the
     * grabber to not be caught up on.
     */
    void doSweep(const Vec2<int>& nextFreeRow, const AFK_Frame& currentFrame);

public:
    AFK_Jigsaw(
        cl_context ctxt,
        const Vec3<int>& _pieceSize,
        const Vec3<int>& _jigsawSize,
        const AFK_JigsawFormatDescriptor *_format,
        GLuint _texTarget,
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
    bool grab(unsigned int threadId, Vec3<int>& o_uvw, AFK_Frame& o_timestamp);

    /* Returns the time your piece was last swept.
     * If you retrieved it at that time or later, you're OK and you
     * can hang on to it.
     */
    AFK_Frame getTimestamp(const AFK_JigsawPiece& piece) const;

    unsigned int getTexCount(void) const;

	/* Returns the (s, t) texture co-ordinates for a given piece
	 * within the jigsaw.  These will be in the range (0, 1).
	 */
	Vec2<float> getTexCoordST(const AFK_JigsawPiece& piece) const;

    /* Returns the (s, t, r) texture co-ordinates for a given piece
     * within the jigsaw.  These will be in the range (0, 1).
     */
    Vec3<float> getTexCoordSTR(const AFK_JigsawPiece& piece) const;

    /* Returns the (s, t) dimensions of one piece within the jigsaw. */
    Vec2<float> getPiecePitchST(void) const;

    /* Returns the (s, t, r) dimensions of one piece within the jigsaw. */
    Vec3<float> getPiecePitchSTR(void) const;

    /* Acquires the buffers for the CL.
     * If cl_gl sharing is enabled, fills out `o_event' with an
     * event to wait for to ensure the textures are acquired.
     */
    cl_mem *acquireForCl(cl_context ctxt, cl_command_queue q, cl_event *o_event);

    /* Releases the buffer from the CL. */
    void releaseFromCl(cl_command_queue q, cl_uint eventsInWaitList, const cl_event *eventWaitList);

    /* Binds a buffer to the GL as a texture.
     * Call once for each texture, having set glActiveTexture()
     * appropriately each time.
     */
    void bindTexture(unsigned int tex);

    /* Signals to the jigsaw to flip the cuboids over and start off
     * a new update cuboid.
     */
    void flipCuboids(const AFK_Frame& currentFrame);
};

#endif /* _AFK_JIGSAW_H_ */

