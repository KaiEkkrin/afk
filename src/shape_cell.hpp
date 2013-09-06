/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_CELL_H_
#define _AFK_SHAPE_CELL_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_solid.hpp"
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
    AFK_ShapeCell();
    virtual ~AFK_ShapeCell();

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

    /* TODO: I suspect that for mega shapes, this is not
     * going to be sustainable, but for now, it seems
     * reasonable to try to allocate the pieces in Shape
     * and have that dole them out.  Less code to change
     * that way.
     * When that's all okay and the workers are running
     * smoothly I can move to a half-cube system here
     * and throw away the need for all cells of a Shape
     * to share the same jigsaw puzzles.
     */
    void assignVapourJigsawPiece(
        const AFK_JigsawPiece& _vapourJigsawPiece,
        const AFK_Frame& _vapourJigsawPieceTimestamp);

    void assignEdgeJigsawPiece(
        const AFK_JigsawPiece& _edgeJigsawPiece,
        const AFK_Frame& _edgeJigsawPieceTimestamp);

    const AFK_JigsawPiece& getVapourJigsawPiece(void) const;
    const AFK_JigsawPiece& getEdgeJigsawPiece(void) const;

    /* Enqueues a display unit for this cell's edge.
     * TODO: A pure vapour render too would be fab!
     */
    void enqueueDisplayUnit(
        const Mat4<float>& worldTransform,
        AFK_JigsawCollection *edgeJigsaws,
        AFK_Fair<AFK_EntityDisplayQueue>& entityDisplayFair) const;

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShapeCell& shapeCell);

#endif /* _AFK_SHAPE_CELL_H_ */

