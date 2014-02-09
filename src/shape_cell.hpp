/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#ifndef _AFK_SHAPE_CELL_H_
#define _AFK_SHAPE_CELL_H_

#include "afk.hpp"

#include <sstream>

#include "3d_solid.hpp"
#include "3d_vapour_compute_queue.hpp"
#include "data/claimable.hpp"
#include "data/evictable_cache.hpp"
#include "data/fair.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw_collection.hpp"
#include "keyed_cell.hpp"
#include "shape_sizes.hpp"


/* A ShapeCell has an artificial max distance set really
 * high, because since all shapes are fully transformed,
 * it won't have a min distance defined by the global
 * minCellSize.
 */
#define SHAPE_CELL_MAX_DISTANCE (1LL<<12)
#define SHAPE_CELL_WORLD_SCALE (1.0f / ((float)SHAPE_CELL_MAX_DISTANCE))

/* A ShapeCell describes one level of detail in a 3D
 * shape, and is cached by the Shape.
 */
class AFK_ShapeCell
{
protected:
    /* A VisibleCell will be used to test visibility and
     * detail pitch, but it will be transient, because
     * these will be different for each Entity that
     * instances the shape.
     * It's the responsibility of the worker function in the
     * `shape' module, I think, to create that VisibleCell
     * and perform the two tests upon it.
     */

    AFK_JigsawPiece vapourJigsawPiece;
    AFK_Frame vapourJigsawPieceTimestamp;

    /* These are this cell's computed min and max densities. */
    float minDensity;
    float maxDensity;

    /* ...and the cell's computed average colour. */
    Vec3<float> avgColour;

    Vec4<float> getBaseColour(int64_t key) const;

public:
    AFK_ShapeCell();

    bool hasVapour(AFK_JigsawCollection *vapourJigsaws) const;

    float getDMin() const { return minDensity; }
    float getDMax() const { return maxDensity; }
    const Vec3<float>& getAvgColour() const { return avgColour; }

    /* Enqueues the compute units.  Both these functions overwrite
     * the relevant jigsaw pieces with new ones.
     * Use the matching VapourCell to build the 3D list required
     * here.
     * In all these cases, `realCoord' should come from the VisibleCell
     * you made for this shape cell (see above)...
     * - TODO: Why is `realCoord' relevant at all?  Surely it shouldn't
     * be (indeed it hardwires objects in place).  With the 3dnet changes
     * I think I should remove it from here, and supply it only to the
     * display queue.
     */
    void enqueueVapourComputeUnitWithNewVapour(
        unsigned int threadId,
        int adjacency,
        const AFK_3DList& list,
        const AFK_KeyedCell& cell,
        const AFK_ShapeSizes& sSizes,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair,
        unsigned int& o_cubeOffset,
        unsigned int& o_cubeCount);

    void enqueueVapourComputeUnitFromExistingVapour(
        unsigned int threadId,
        int adjacency,
        unsigned int cubeOffset,
        unsigned int cubeCount,
        const AFK_KeyedCell& cell,
        const AFK_ShapeSizes& sSizes,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair);

    /* Updates this cell's density information (call from
     * dreduce)
     */
    void setDMinMax(float _minDensity, float _maxDensity);

    /* Likewise for the average colour. */
    void setAvgColour(const Vec3<float>& _avgColour);

    /* For handling claiming and eviction. */
    void evict(void);

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);

#endif /* _AFK_SHAPE_CELL_H_ */

