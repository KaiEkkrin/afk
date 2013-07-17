/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_CELL_H_
#define _AFK_WORLD_CELL_H_

#include "afk.hpp"

#include "data/claimable.hpp"
#include "world.hpp"


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

    /* AFK_Claimable implementation. */
    virtual AFK_Frame getCurrentFrame(void) const;

    /* Says whether this cell can be evicted from the cache.
     */
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_WorldCell& worldCell);

#endif /* _AFK_WORLD_CELL_H_ */

