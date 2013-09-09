/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_VAPOUR_CELL_H_
#define _AFK_VAPOUR_CELL_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include "3d_solid.hpp"
#include "cell.hpp"
#include "data/claimable.hpp"
#include "data/frame.hpp"
#include "data/polymer_cache.hpp"
#include "shape_sizes.hpp"

#ifndef AFK_VAPOUR_CELL_CACHE
#define AFK_VAPOUR_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_VapourCell, AFK_HashCell>
#endif

/* To access a vapour cell cache, use this to transform shape
 * cell coords to vapour cell.
 */
AFK_Cell afk_vapourCell(const AFK_Cell& cell);

/* A VapourCell describes a cell in a shape with its vapour
 * features and cubes.  I'm making it distinct from a ShapeCell
 * because VapourCells exist at larger scales, crossing multiple
 * ShapeCells.
 */
class AFK_VapourCell: public AFK_Claimable
{
protected:
    AFK_Cell cell;

    /* The actual features here. */
    bool haveDescriptor;
    std::vector<AFK_3DVapourFeature> features;
    std::vector<AFK_3DVapourCube> cubes;

    /* These fields track whether this vapour cell has already
     * been pushed into the vapour compute queue this round,
     * keeping hold of the cube offset and count if it
     * has.
     */
    unsigned int computeCubeOffset;
    unsigned int computeCubeCount;
    AFK_Frame computeCubeFrame;

public:
    /* Binds a shape cell to the shape. */
    void bind(const AFK_Cell& _cell);

    bool hasDescriptor(void) const;

    /* Sorts out the vapour descriptor.  Do this under an
     * exclusive claim.
     */
    void makeDescriptor(
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes);

    /* Builds the 3D list for this cell.
     * Fills the vector `missingCells' with the list of cells
     * whose vapour descriptor needs to be created first in
     * order to be able to make this list.
     */
    void build3DList(
        unsigned int threadId,
        AFK_3DList& list,
        std::vector<AFK_Cell>& missingCells,
        unsigned int subdivisionFactor,
        const AFK_VAPOUR_CELL_CACHE *cache);

    /* Checks whether this vapour cell's features have
     * already gone into the compute queue this frame.
     * If they have, all that you need is the provided
     * offsets.
     */
    bool alreadyEnqueued(
        unsigned int& o_cubeOffset,
        unsigned int& o_cubeCount) const;

    /* If the above retuned false, you need to build the
     * 3D list, do an extend on the queue and (while still
     * holding your exclusive claim) call this to push
     * the queue position.
     */
    void enqueued(
        unsigned int cubeOffset,
        unsigned int cubeCount);

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);

#endif /* _AFK_VAPOUR_CELL_H_ */

