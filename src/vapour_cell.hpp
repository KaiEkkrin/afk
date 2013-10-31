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
 * translate cell co-ordinates.
 */
AFK_KeyedCell afk_shapeToVapourCell(const AFK_KeyedCell& cell, const AFK_ShapeSizes& sSizes);

/* A VapourCell describes a cell in a shape with its vapour
 * features and cubes.  I'm making it distinct from a ShapeCell
 * because VapourCells exist at larger scales, crossing multiple
 * ShapeCells.
 */
class AFK_VapourCell
{
public:
    /* For the polymer. */
    boost::atomic<AFK_KeyedCell> key;
    AFK_Claimable claimable;

protected:
    /* The actual features here. */
    bool haveDescriptor;
    AFK_Skeleton *skeleton;
    std::vector<AFK_3DVapourFeature> *features;
    std::vector<AFK_3DVapourCube> *cubes;

    /* These fields track whether this vapour cell has already
     * been pushed into the vapour compute queue this round,
     * keeping hold of the cube offset and count if it
     * has.
     */
    unsigned int computeCubeOffset;
    unsigned int computeCubeCount;
    AFK_Frame computeCubeFrame;

public:
    AFK_VapourCell();
    virtual ~AFK_VapourCell();

    const AFK_KeyedCell getCell(void) const { return key.load(); }

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

    /* You can check whether a specific ShapeCell is within
     * the skeleton, so long as a descriptor is present ...
     */
    bool withinSkeleton(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const;

    /* ...and obtain its adjacency (flags; 0-5 inclusive; in the
     * usual order).
     */
    int skeletonAdjacency(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const;

    /* As above, but returns the full adjacency (see description
     * in skeleton.hpp)
     */
    int skeletonFullAdjacency(const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes) const;

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
     * Fills the vector `missingCells' with the list of *vapour* cells
     * whose vapour descriptor needs to be created first in
     * order to be able to make this list.  They go from
     * smallest to largest cell.
     */
    void build3DList(
        unsigned int threadId,
        AFK_3DList& list,
        const AFK_ShapeSizes& sSizes,
        const AFK_VAPOUR_CELL_CACHE *cache) const;

    /* Checks whether this vapour cell's features have
     * already gone into the compute queue this frame.
     * If they have, all that you need is the provided
     * offsets.
     */
    bool alreadyEnqueued(
        unsigned int& o_cubeOffset,
        unsigned int& o_cubeCount) const;

    /* If the above returned false, you need to build the
     * 3D list, do an extend on the queue and (while still
     * holding your exclusive claim) call this to push
     * the queue position.
     */
    void enqueued(
        unsigned int cubeOffset,
        unsigned int cubeCount);

    /* For handling claiming and eviction. */
    bool canBeEvicted(void) const;
    void evict(void);

    friend std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_VapourCell& vapourCell);

#endif /* _AFK_VAPOUR_CELL_H_ */

