/* AFK
 * Copyright (C) 2013, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#ifndef _AFK_JIGSAW_H_
#define _AFK_JIGSAW_H_

#include "afk.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>

#include <boost/lockfree/queue.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "compute_dependency.hpp"
#include "computer.hpp"
#include "data/frame.hpp"
#include "data/moving_average.hpp"
#include "jigsaw_cuboid.hpp"
#include "jigsaw_image.hpp"

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

size_t hash_value(const AFK_JigsawPiece& jigsawPiece);

std::ostream& operator<<(std::ostream& os, const AFK_JigsawPiece& piece);

BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_JigsawPiece>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_JigsawPiece>::value));


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
    std::vector<AFK_JigsawImage*> images;
    const Vec3<int> jigsawSize; /* number of pieces in each dimension */

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
     * Note that all the protected utility functions assume that
     * the right `cuboidMut' has already been acquired appropriately.
     * There's one each for the update and draw cuboids, flipping just
     * like the cuboid vectors themselves.
     */
    const unsigned int concurrency;
    std::vector<AFK_JigsawCuboid> cuboids[2];
    unsigned int updateCs;
    unsigned int drawCs;
    boost::upgrade_mutex cuboidMuts[2];

    /* This maps caller thread IDs (declared to us in the constructor)
     * to numbers 0..concurrency.
     * All internal protected functions that take a thread ID parameter
     * will take one that has already been looked up in the map.
     */
    std::map<unsigned int, unsigned int> threadIdMap;

    /* This is the sweep position, which tracks ahead of the update
     * cuboids.  Every flip, we should move the sweep position up and back
     * and re-timestamp all the rows it passes, telling the enumerators
     * to-regenerate the pieces of any tiles that still have pieces
     * within the sweep.
     */
    Vec2<int> sweepPosition;

    /* This is the average number of columns that cuboids seem to
     * be using.  It's used as a heuristic to decide whether to start
     * cuboids on new rows or not.
     * Update it in flipCuboids().
     */
    AFK_MovingAverage<int> columnCounts;

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
        AFK_Frame *o_timestamp);

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

    /* Some internal stats: */
    boost::atomic<uint64_t> piecesGrabbed;
    boost::atomic<uint64_t> cuboidsStarted;
    boost::atomic<uint64_t> piecesSwept;

public:
    AFK_Jigsaw(
        AFK_Computer *_computer,
        const Vec3<int>& _jigsawSize,
        const std::vector<AFK_JigsawImageDescriptor>& _desc,
        const std::vector<unsigned int>& _threadIds);
    virtual ~AFK_Jigsaw();

    /* Acquires a new piece for your thread.
     * If this function returns false, this jigsaw has run out of
     * space and the JigsawCollection needs to use a different one.
     * This function should be thread safe so long as you don't
     * lie about your thread ID
     */
    bool grab(unsigned int threadId, Vec3<int>& o_uvw, AFK_Frame *o_timestamp);

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

    /* Get fake 3D info for the jigsaw in a format suitable for
     * sending to the CL.  (-1s will be returned if fake 3D is
     * not in use.)
     */
    Vec2<int> getFake3D_size(unsigned int tex) const;
    int getFake3D_mult(unsigned int tex) const;

    /* TODO: For the next two functions -- I'm now going around re-
     * acquiring jigsaws for read after having first acquired them
     * for write, and all sorts.  In which case I don't need to re-blit
     * all the same data.  Consider optimising so that data is only
     * copied over to the GL after a write acquire is released?
     */

    /* Acquires an image for the CL.
     * Fills out the given compute dependency as required.
     */
    cl_mem acquireForCl(unsigned int tex, AFK_ComputeDependency& o_dep);

    /* Releases an image from the CL,
     * waiting for the given dependency.
     */
    void releaseFromCl(unsigned int tex, const AFK_ComputeDependency& dep);

    /* Binds an image to the GL as a texture.
     * Expects you to have set glActiveTexture() appropriately first!
     */
    void bindTexture(unsigned int tex);

    /* Signals to the jigsaw to flip the cuboids over and start off
     * a new update cuboid.
     */
    void flipCuboids(const AFK_Frame& currentFrame);

    void printStats(std::ostream& os, const std::string& prefix);
};

#endif /* _AFK_JIGSAW_H_ */

