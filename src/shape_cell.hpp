/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_CELL_H_
#define _AFK_SHAPE_CELL_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_edge_compute_queue.hpp"
#include "3d_solid.hpp"
#include "3d_vapour_compute_queue.hpp"
#include "cell.hpp"
#include "data/claimable.hpp"
#include "data/fair.hpp"
#include "data/frame.hpp"
#include "data/polymer_cache.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw_collection.hpp"
#include "shape_sizes.hpp"


/* A ShapeCell has an artificial max distance set really
 * high, because since all shapes are fully transformed,
 * it won't have a min distance defined by the global
 * minCellSize.
 */
#define SHAPE_CELL_MAX_DISTANCE (1LL<<30)
#define SHAPE_CELL_WORLD_SCALE (1.0f / ((float)SHAPE_CELL_MAX_DISTANCE))

#ifndef AFK_SHAPE_CELL_CACHE
#define AFK_SHAPE_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_ShapeCell, AFK_HashCell>
#endif

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
    AFK_Cell cell;

    Mat4<float> getCellToShapeTransform(void) const;

    /* This cell's vapour information... */
    bool haveVapourDescriptor;
    std::vector<AFK_3DVapourFeature> vapourFeatures;
    std::vector<AFK_3DVapourCube> vapourCubes;

    AFK_JigsawPiece vapourJigsawPiece;
    AFK_Frame vapourJigsawPieceTimestamp;

    /* ...and edge information */
    AFK_JigsawPiece edgeJigsawPiece;
    AFK_Frame edgeJigsawPieceTimestamp;

public:
    /* Binds a shape cell to the shape. */
    void bind(const AFK_Cell& _cell);
    const AFK_Cell& getCell(void) const;

    bool hasVapourDescriptor(void) const;
    void makeVapourDescriptor(
        unsigned int shapeKey,
        const AFK_ShapeSizes& sSizes);

    bool hasVapour(const AFK_JigsawCollection *vapourJigsaws) const;
    bool hasEdges(const AFK_JigsawCollection *edgeJigsaws) const;

    /* Builds the 3D list for this cell.
     */
    void build3DList(
        unsigned int threadId,
        AFK_3DList& list,
        unsigned int subdivisionFactor,
        const AFK_SHAPE_CELL_CACHE *cache);

    /* Enqueues the compute units.  Both these functions overwrite
     * the relevant jigsaw pieces with new ones.
     * TODO: Cross-reference, over-compute, stitching, and all
     * that junk.  Have a think.  I really do believe I want to
     * avoid cross referencing at the cell level, and I *like*
     * the half-cube paradigm (as demonstrated by its successful
     * terrain equivalent, the half-tile).
     * I could consider adding an `adjacency' compute step, or
     * something.
     */
    void enqueueVapourComputeUnit(
        unsigned int threadId,
        const AFK_3DList& list,
        const AFK_ShapeSizes& sSizes,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_Fair<AFK_3DVapourComputeQueue>& vapourComputeFair);

    void enqueueEdgeComputeUnit(
        unsigned int threadId,
        AFK_JigsawCollection *vapourJigsaws,
        AFK_JigsawCollection *edgeJigsaws,
        AFK_Fair<AFK_3DEdgeComputeQueue>& edgeComputeFair);   

    /* Enqueues an edge display unit for this cell's edge.
     * TODO: A pure vapour render too would be fab!
     */
    void enqueueEdgeDisplayUnit(
        const Mat4<float>& worldTransform,
        const AFK_JigsawCollection *edgeJigsaws,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const;

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);

#endif /* _AFK_SHAPE_CELL_H_ */

