/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_CELL_H_
#define _AFK_WORLD_CELL_H_

#include "afk.hpp"

#include "frame.hpp"
#include "terrain.hpp"
#include "world.hpp"


class AFK_DisplayedWorldCell;
class AFK_WorldCache;

#define TERRAIN_CELLS_PER_CELL 5

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

    /* The identity of the thread that last claimed use of
     * this cell.
     */
    boost::thread::id claimingThreadId;

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

    /* Internal terrain computation. */
    void computeTerrainRec(Vec3<float> *positions, Vec3<float> *colours, size_t length, AFK_WorldCache *cache) const;

public:
    /* The data for this cell's landscape, if we've
     * calculated it.
     */
    AFK_DisplayedWorldCell *displayed;

    AFK_WorldCell();
    virtual ~AFK_WorldCell();

    const Vec4<float>& getRealCoord(void) const { return realCoord; }

    /* Tries to claim this landscape cell for processing by
     * the current thread.
     * I have no idea whether this is correct.  If something
     * kerpows, maybe it isn't...
     */
    bool claim(void);

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

    /* Says whether this cell can be evicted from the cache. */
    bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& landscapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& landscapeCell);

#endif /* _AFK_WORLD_CELL_H_ */

