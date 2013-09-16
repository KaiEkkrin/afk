/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_VAPOUR_CELL_H_
#define _AFK_VAPOUR_CELL_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include "3d_solid.hpp"
#include "data/claimable.hpp"
#include "data/evictable_cache.hpp"
#include "data/frame.hpp"
#include "keyed_cell.hpp"
#include "shape_sizes.hpp"
#include "skeleton.hpp"

#ifndef AFK_VAPOUR_CELL_CACHE
#define AFK_VAPOUR_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_VapourCell, AFK_HashKeyedCell>
#endif

/* To access a vapour cell cache, use afk_shapeToVapourCell() to
 * translate cell co-ordinates.  The other function goes the other
 * way!
 */
AFK_KeyedCell afk_shapeToVapourCell(const AFK_KeyedCell& cell, const AFK_ShapeSizes& sSizes);
AFK_KeyedCell afk_vapourToShapeCell(const AFK_KeyedCell& cell, const AFK_ShapeSizes& sSizes);

/* A VapourCell describes a cell in a shape with its vapour
 * features and cubes.  I'm making it distinct from a ShapeCell
 * because VapourCells exist at larger scales, crossing multiple
 * ShapeCells.
 */
class AFK_VapourCell: public AFK_Claimable
{
protected:
    AFK_KeyedCell cell;

    /* The actual features here. */
    bool haveDescriptor;
    AFK_Skeleton skeleton;
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
    void bind(const AFK_KeyedCell& _cell);

    bool hasDescriptor(void) const;

    /* Sorts out the vapour descriptor.  Do this under an
     * exclusive claim.
     * This one makes a top-level vapour cell.
     * Fills out `bones' with a list of the SkeletonCubes
     * so that the caller can turn these into shape cells
     * to enqueue.
     */
    void makeDescriptor(const AFK_ShapeSizes& sSizes);

    /* This one makes a finer detail vapour cell, whose
     * skeleton is derived from the upper cell.
     */
    void makeDescriptor(
        const AFK_VapourCell& upperCell,
        const AFK_ShapeSizes& sSizes);

    /* Having made the descriptor, you can check whether this
     * vapour cell is actually within the skeleton by calling
     * this.
     * If it isn't, just bypass all the subsequent rendering
     * stuff and reject this cell...
     */
    bool withinSkeleton(void) const;

    /* This enumerates the shape cells that compose the bones of
     * the skeleton here, so that they can be easily enqueued.
     */
    class ShapeCells
    {
    protected:
        AFK_Skeleton::Bones bones;
        const AFK_VapourCell& vapourCell;
        const AFK_ShapeSizes& sSizes;

    public:
        ShapeCells(const AFK_VapourCell& _vapourCell, const AFK_ShapeSizes& _sSizes);

        bool hasNext(void);
        AFK_KeyedCell next(void);
    };

    /* Builds the 3D list for this cell.
     * Fills the vector `missingCells' with the list of cells
     * whose vapour descriptor needs to be created first in
     * order to be able to make this list.  They go from
     * smallest to largest cell, and are in shape space (as
     * expected by shape workers).
     */
    void build3DList(
        unsigned int threadId,
        AFK_3DList& list,
        std::vector<AFK_KeyedCell>& missingCells,
        const AFK_ShapeSizes& sSizes,
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
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);

#endif /* _AFK_VAPOUR_CELL_H_ */

