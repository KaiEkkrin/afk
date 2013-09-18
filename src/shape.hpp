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
#include "data/evictable_cache.hpp"
#include "data/fair.hpp"
#include "data/frame.hpp"
#include "entity_display_queue.hpp"
#include "object.hpp"
#include "jigsaw_collection.hpp"
#include "shape_cell.hpp"
#include "vapour_cell.hpp"
#include "visible_cell.hpp"
#include "work.hpp"

enum AFK_ShapeArtworkState
{
    AFK_SHAPE_NO_PIECE_ASSIGNED,
    AFK_SHAPE_PIECE_SWEPT,
    AFK_SHAPE_HAS_ARTWORK
};

#ifndef AFK_SHAPE_CELL_CACHE
#define AFK_SHAPE_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_ShapeCell, AFK_HashKeyedCell>
#endif

#ifndef AFK_VAPOUR_CELL_CACHE
#define AFK_VAPOUR_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_VapourCell, AFK_HashKeyedCell>
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
 * Also unlike the others, this one expects the `cell' field
 * to contain a *vapour* cell location, not a shape cell one
 * (see the function `afk_shapeToVapourCell').
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

/* This class has changed, and now describes all shapes.
 * (It turned out that all information specific to single
 * shapes ended up in ShapeCell and VapourCell.)
 */
class AFK_Shape
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
        const AFK_VisibleCell& visibleCell,
        AFK_ShapeCell& shapeCell,
        enum AFK_ClaimStatus& claimStatus,
        unsigned int threadId,
        const struct AFK_WorldWorkParam::Shape& param,
        AFK_WorldWorkQueue& queue);

    /* TODO: Try to move the shape-dependent stuff out of
     * `world' into here, so that I can stop sending along
     * the world pointer.
     * But that's a bit more of a long term cleanup task.
     */
    AFK_SHAPE_CELL_CACHE *shapeCellCache;
    AFK_VAPOUR_CELL_CACHE *vapourCellCache;

public:
    AFK_Shape(
        const AFK_Config *config,
        unsigned int shapeCacheSize);
    virtual ~AFK_Shape();

    void updateWorld(void);
    void printCacheStats(std::ostream& os, const std::string& prefix);

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
};


#endif /* _AFK_SHAPE_H_ */

