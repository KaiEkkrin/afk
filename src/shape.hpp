/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHAPE_H_
#define _AFK_SHAPE_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "3d_solid.hpp"
#include "data/claimable.hpp"
#include "data/fair.hpp"
#include "data/frame.hpp"
#include "data/polymer_cache.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "jigsaw_collection.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"
#include "work.hpp"

enum AFK_ShapeArtworkState
{
    AFK_SHAPE_NO_PIECE_ASSIGNED,
    AFK_SHAPE_PIECE_SWEPT,
    AFK_SHAPE_HAS_ARTWORK
};

#ifndef AFK_SHAPE_CELL_CACHE
#define AFK_SHAPE_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_ShapeCell, AFK_HashCell>
#endif

#ifndef AFK_VAPOUR_CELL_CACHE
#define AFK_VAPOUR_CELL_CACHE AFK_PolymerCache<AFK_Cell, AFK_VapourCell, AFK_HashCell>
#endif

/* This is the top level entity worker.  It makes sure that
 * the top vapour cell has been made and then uses that to
 * derive the list of top level shape cells and enqueue them
 * (by generateShapeCells).
 * You should enqueue it with the top level cell (0, 0, 0,
 * SHAPE_CELL_MAX_DISTANCE).
 */
bool afk_generateEntity(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue);

/* This is a really simple worker that just generates the
 * vapour descriptor and nothing else.
 * Needed to fill out 3D lists for more detailed vapour
 * cells whose less-detailed versions have never been hit.
 * This one generates finer-LoD vapour, not top-level (see
 * above).
 */
bool afk_generateVapourDescriptor(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue);

/* Queued into the world work queue, this function generates
 * shape cells.
 */
bool afk_generateShapeCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue);

/* A Shape describes a single 3D shape, which
 * might be instanced many times by means of Entities.
 */
class AFK_Shape: public AFK_Claimable
{
protected:
    /* Possible return states for the below. */
    enum VapourCellState
    {
        Enqueued,
        NeedsResume,
        Empty
    };

    /* Builds the vapour for a claimed shape cell.  Sorts out the
     * vapour cell business.
     */
    enum VapourCellState enqueueVapourCell(
        unsigned int threadId,
        unsigned int shapeKey,
        AFK_ShapeCell& shapeCell,
        const struct AFK_WorldWorkParam::Shape& param,
        AFK_WorldWorkQueue& queue);

    /* Generates a claimed shape cell at its level of detail. */
    void generateClaimedShapeCell(
        AFK_ShapeCell& shapeCell,
        enum AFK_ClaimStatus& claimStatus,
        unsigned int threadId,
        const struct AFK_WorldWorkParam::Shape& param,
        AFK_WorldWorkQueue& queue);

    /* TODO I'm already getting issues with these.  I'm pretty sure
     * there should be single, global shape cell and vapour cell
     * caches instead, which means storing them in World and
     * indexing them by (cell, shape key).  Which will require some
     * kind of `KeyedCell' class...
     * Shouldn't be hard really. :P
     * ... actually ...
     * Look: I'm about to remove the last vestiges of state from Shape.
     * The worker code in this module can all just move back into World
     * (yeah, it'll push it over 1000 lines again but that's okay really)
     * and this object can be entirely removed.
     */
    AFK_SHAPE_CELL_CACHE *shapeCellCache;
    AFK_VAPOUR_CELL_CACHE *vapourCellCache;

public:
    AFK_Shape();
    virtual ~AFK_Shape();

    /* For handling claiming and eviction. */
    virtual AFK_Frame getCurrentFrame(void) const;
    virtual bool canBeEvicted(void) const;

    friend bool afk_generateEntity(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        AFK_WorldWorkQueue& queue);

    friend bool afk_generateVapourDescriptor(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        AFK_WorldWorkQueue& queue);

    friend bool afk_generateShapeCells(
        unsigned int threadId,
        const union AFK_WorldWorkParam& param,
        AFK_WorldWorkQueue& queue);

    friend std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);
};

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape);


#endif /* _AFK_SHAPE_H_ */

