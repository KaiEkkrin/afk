/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_CELL_H_
#define _AFK_WORLD_CELL_H_

#include "afk.hpp"

#include <exception>

#include <boost/atomic.hpp>

#include "frame.hpp"
#include "terrain.hpp"
#include "world.hpp"


class AFK_DisplayedWorldCell;
class AFK_WorldCache;

#define TERRAIN_CELLS_PER_CELL 5


/* This occurs if we can't find a cell while trying to chain
 * together the terrain.
 */
class AFK_WorldCellNotPresentException: public std::exception {};

/* These are the possible results from an attempt to claim a
 * cell.
 */
enum AFK_WorldCellClaimStatus
{
    AFK_WCC_CLAIMED,                /* You got it */
    AFK_WCC_ALREADY_PROCESSED,      /* It's already been processed this frame */
    AFK_WCC_TAKEN                   /* It hasn't been processed this frame but someone else has it right now */
};

/* Describes one cell in the world.  This is the value that we
 * cache in the big ol' WorldCache.
 */
class AFK_WorldCell
{
protected:
    /* This value says when the subcell enumerator last used
     * this cell.  If it's a long time ago, the cell will
     * become a candidate for eviction from the cache.
     */
    AFK_Frame lastSeen;

    /* The identity of the thread that has claimed use of
     * this cell.
     */
    boost::atomic<unsigned int> claimingThreadId;

    /* The obvious. */
    AFK_Cell cell;

    /* If this is a top level cell, don't go hunting in its parent
     * for stuff.
     */
    bool topLevel;

    /* The matching world co-ordinates.
     * TODO This is the thing that needs to change everywhere
     * when doing a rebase...  Of course, though since the
     * gen worker calls bind() on every cell it touches maybe
     * it will be easy...
     */
    Vec4<float> realCoord;

    /* The terrain at this cell.
     * There's one terrain cell that corresponds to this cell proper,
     * and four that correspond to a 1/2 cell offset in each diagonal
     * of the x-z plane.
     * Well-known RNG seeding keeps these consistent across cell seams.
     */
    bool hasTerrain;
    AFK_TerrainCell terrain[TERRAIN_CELLS_PER_CELL];

    /* Internal terrain computation.
     * TODO This ought to be split apart: I should first compile
     * a list of all the relevant terrain cells, and only after
     * that, do the computation on the whole list.
     * Like that, I can avoid doing lots of computation before
     * finding myself throwing an AFK_WorldCellNotPresentException,
     * and it would also make the terrain computation more amenable
     * to being OpenCLified.
     */
    void computeTerrainRec(Vec3<float> *positions, Vec3<float> *colours, size_t length, AFK_WorldCache *cache) const;

public:
    /* The data for this cell's landscape, if we've
     * calculated it.
     */
    AFK_DisplayedWorldCell *displayed;

    AFK_WorldCell();
    virtual ~AFK_WorldCell();

    const AFK_Cell& getCell(void) const { return cell; }
    const Vec4<float>& getRealCoord(void) const { return realCoord; }

    /* Tries to claim this landscape cell for processing.
     * When finished, release it by calling release().
     * The flag says whether to update the lastSeen field:
     * only one claim can do this per frame, so claims for
     * spillage or eviction shouldn't set this.
     */
    enum AFK_WorldCellClaimStatus claim(unsigned int threadId, bool touch);

    void release(unsigned int threadId);

    /* Helper -- tries a bit harder to claim the cell.
     * Returns true if success, else false
     */
    bool claimYieldLoop(unsigned int threadId, bool touch);

    /* Binds a landscape cell to the world.  Needs to be called
     * before anything else gets done, because WorldCells
     * are created uninitialised in the cache.
     */
    void bind(const AFK_Cell& _cell, bool _topLevel, float worldScale);

    /* Tests whether this cell is within the specified detail pitch
     * when viewed from the specified location.
     */
    bool testDetailPitch(float detailPitch, const AFK_Camera& camera, const Vec3<float>& viewerLocation) const;

    /* Tests whether none, some or all of this cell's vertices are
     * visible when projected with the supplied camera.
     */
    void testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const;

    /* Adds terrain if there isn't any already. */
    void makeTerrain(
        unsigned int pointSubdivisionFactor,
        unsigned int subdivisionFactor,
        float minCellSize,
        const Vec3<float> *forcedTint);

    /* Computes the total terrain features here. */
    void computeTerrain(Vec3<float> *positions, Vec3<float> *colours, size_t length, AFK_WorldCache *cache) const;

    /* Gets the AFK_Cell pointing to the cell that "owns"
     * this one's geometry.
     */
    AFK_Cell terrainRoot(void) const;

    /* Says whether this cell can be evicted from the cache.
     * Note that you need to check all the spill cells as well,
     * not just this one!
     */
    bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& landscapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& landscapeCell);

#endif /* _AFK_WORLD_CELL_H_ */

