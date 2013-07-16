/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_CELL_H_
#define _AFK_WORLD_CELL_H_

#include "afk.hpp"

#include <exception>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/shared_ptr.hpp>

#include "data/claimable.hpp"
#include "terrain.hpp"
#include "world.hpp"


#define TERRAIN_CELLS_PER_CELL 5

#ifndef AFK_WORLD_CACHE
#define AFK_WORLD_CACHE AFK_EvictableCache<AFK_Cell, AFK_WorldCell, AFK_HashCell>
#endif


/* This occurs if we can't find a cell while trying to chain
 * together the terrain.
 */
class AFK_WorldCellNotPresentException: public std::exception {};

/* Describes one cell in the world.  This is the value that we
 * cache in the big ol' WorldCache.
 */
class AFK_WorldCell: public AFK_Claimable
{
private:
    /* These things shouldn't be going on */
    AFK_WorldCell(const AFK_WorldCell& other) {}
    AFK_WorldCell& operator=(const AFK_WorldCell& other) { return *this; }

protected:
    /* The obvious. */
    AFK_Cell cell;

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
    std::vector<boost::shared_ptr<AFK_TerrainCell> > terrain;

public:
    AFK_WorldCell();

    const AFK_Cell& getCell(void) const { return cell; }
    const Vec4<float>& getRealCoord(void) const { return realCoord; }

    /* Binds a landscape cell to the world.  Needs to be called
     * before anything else gets done, because WorldCells
     * are created uninitialised in the cache.
     */
    void bind(const AFK_Cell& _cell, float worldScale);

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

    /* Builds a terrain list for this cell.
     * Call it with empty lists.
     * Fills out the `missing' list with the cells that
     * are missing from the cache:
     * you'll need to process them then try again.
     */
    void buildTerrainList(
        AFK_TerrainList& list,
        std::vector<AFK_Cell>& missing,
        float maxDistance,
        const AFK_WORLD_CACHE *cache) const;

    /* Gets the AFK_Cell pointing to the cell that "owns"
     * this one's geometry.
     */
    AFK_Cell terrainRoot(void) const;

    /* AFK_Claimable implementation. */
    virtual AFK_Frame getCurrentFrame(void) const;

    /* Says whether this cell can be evicted from the cache.
     */
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& landscapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& landscapeCell);

#endif /* _AFK_WORLD_CELL_H_ */

