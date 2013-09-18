/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_CELL_H_
#define _AFK_SHAPE_CELL_H_

#include "afk.hpp"

#include <sstream>

#include "3d_edge_compute_queue.hpp"
#include "3d_solid.hpp"
#include "3d_vapour_compute_queue.hpp"
#include "data/claimable.hpp"
#include "data/fair.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw_collection.hpp"
#include "keyed_cell.hpp"
#include "shape_sizes.hpp"

#ifndef AFK_SHAPE_CELL_CACHE
#define AFK_SHAPE_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_ShapeCell, AFK_HashKeyedCell>
#endif


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
class AFK_ShapeCell: public AFK_Claimable
{
protected:
    /* Location information in shape space.
     * A VisibleCell will be used to test visibility and
     * detail pitch, but it will be transient, because
     * these will be different for each Entity that
     * instances the shape.
     * It's the responsibility of the worker function in the
     * `shape' module, I think, to create that VisibleCell
     * and perform the two tests upon it.
     */
    AFK_KeyedCell cell;

    AFK_JigsawPiece vapourJigsawPiece;
    AFK_Frame vapourJigsawPieceTimestamp;

    /* ...and edge information */
    AFK_JigsawPiece edgeJigsawPiece;
    AFK_Frame edgeJigsawPieceTimestamp;

public:
    /* Binds a shape cell to the shape. */
    void bind(const AFK_KeyedCell& _cell);
    const AFK_KeyedCell& getCell(void) const;

    bool hasVapour(AFK_JigsawCollection *vapourJigsaws) const;
    bool hasEdges(AFK_JigsawCollection *edgeJigsaws) const;

    bool getVapourJigsawPiece(AFK_JigsawCollection *vapourJigsaws, AFK_JigsawPiece *o_jigsawPiece) const;

    /* Enqueues the compute units.  Both these functions overwrite
     * the relevant jigsaw pieces with new ones.
     * Use the matching VapourCell to build the 3D list required
     * here.
     * TODO: Cross-reference, over-compute, stitching, and all
     * that junk.  Have a think.  I really do believe I want to
     * avoid cross referencing at the cell level, and I *like*
     * the half-cube paradigm (as demonstrated by its successful
     * terrain equivalent, the half-tile).
     * I could consider adding an `adjacency' compute step, or
     * something.
     */
    void enqueueVapourComputeUnitWithNewVapour(
        unsigned int threadId,
        const AFK_3DList& list,
        const AFK_ShapeSizes& sSizes,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair,
        unsigned int& o_cubeOffset,
        unsigned int& o_cubeCount);

    void enqueueVapourComputeUnitFromExistingVapour(
        unsigned int threadId,
        unsigned int cubeOffset,
        unsigned int cubeCount,
        const AFK_ShapeSizes& sSizes,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair);

    /* This function fills out `missingCells' with the missing 
     * cells that it needs for vapour adjacency information if it can't find
     * all the necessary.
     * You should compute those and resume.
     */
    void enqueueEdgeComputeUnit(
        unsigned int threadId,
        std::vector<AFK_KeyedCell>& missingCells,
        const AFK_SHAPE_CELL_CACHE *cache,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_JigsawCollection *edgeJigsaws,
        AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair);   

    /* Enqueues an edge display unit for this cell's edge.
     * TODO: A pure vapour render too would be fab!
     */
    void enqueueEdgeDisplayUnit(
        const Mat4<float>& worldTransform,
        AFK_JigsawCollection *edgeJigsaws,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const;

    /* For handling claiming and eviction. */
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);

#endif /* _AFK_SHAPE_CELL_H_ */

