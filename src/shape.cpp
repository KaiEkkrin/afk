/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <cstring>

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>

#include "camera.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"
#include "rng/aes.hpp"
#include "shape.hpp"


/* Shape worker flags. */
#define AFK_SCG_FLAG_ENTIRELY_VISIBLE       1

/* The top-level entity worker. */
bool afk_generateEntity(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue)
{
    AFK_KeyedCell cell                      = param.shape.cell;
    AFK_World *world                        = param.shape.world;

    AFK_Shape& shape                        = world->shape;

    bool resume = false;

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
    AFK_VapourCell& vapourCell = (*(shape.vapourCellCache))[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_UPGRADE, afk_core.computingFrame);

    if (!vapourCell.hasDescriptor())
    {
        /* Get an exclusive claim, and make its descriptor. */
        claimStatus = vapourCell.upgrade(threadId, claimStatus);

        if (claimStatus == AFK_CL_CLAIMED)
        {
            if (!vapourCell.hasDescriptor())
            {
                vapourCell.makeDescriptor(world->sSizes);
                world->separateVapoursComputed.fetch_add(1);
            }        
        }
        else
        {
            /* I'd better resume this later. */
            resume = true;
        }
    }

    if (vapourCell.hasDescriptor())
    {
        /* Go through the skeleton... */
        AFK_VapourCell::ShapeCells shapeCells(vapourCell, world->sSizes);
        while (shapeCells.hasNext())
        {
            AFK_KeyedCell nextCell = shapeCells.next();

            /* Enqueue this shape cell. */
            AFK_WorldWorkQueue::WorkItem shapeCellItem;
            shapeCellItem.func = afk_generateShapeCells;
            shapeCellItem.param = param;
            shapeCellItem.param.shape.cell = nextCell;
            shapeCellItem.param.shape.dependency = NULL;
            queue.push(shapeCellItem);
        }
    }

    vapourCell.release(threadId, claimStatus);

    if (resume)
    {
        AFK_WorldWorkQueue::WorkItem resumeItem;
        resumeItem.func = afk_generateEntity;
        resumeItem.param = param;
        if (param.shape.dependency) param.shape.dependency->retain();
        queue.push(resumeItem);
    }

    /* If this cell had a dependency ... */
    if (param.shape.dependency)
    {
        if (param.shape.dependency->check(queue))
        {
            world->dependenciesFollowed.fetch_add(1);
            delete param.shape.dependency;
        }
    }

    return true;
}

/* The shape worker */

#define VISIBLE_CELL_DEBUG 0
#define NONZERO_CELL_DEBUG 0

bool afk_generateShapeCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_KeyedCell cell                = param.shape.cell;
    AFK_Entity *entity                      = param.shape.entity;
    AFK_World *world                        = param.shape.world;
    const Vec3<float>& viewerLocation       = param.shape.viewerLocation;
    const AFK_Camera *camera                = param.shape.camera;

    bool entirelyVisible                    = ((param.shape.flags & AFK_SCG_FLAG_ENTIRELY_VISIBLE) != 0);

    AFK_Shape& shape                        = world->shape;
    Mat4<float> worldTransform              = entity->obj.getTransformation();

    /* Next, handle the shape cell ... */
    AFK_ShapeCell& shapeCell = (*(shape.shapeCellCache))[cell];
    shapeCell.bind(cell);
    AFK_ClaimStatus claimStatus = shapeCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);

    bool recurse = false, resume = false;

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;
    AFK_VisibleCell visibleCell;
    visibleCell.bindToCell(cell, SHAPE_CELL_WORLD_SCALE, worldTransform);

#if VISIBLE_CELL_DEBUG
    AFK_DEBUG_PRINTL("testing shape cell " << cell << ": visible cell " << visibleCell)
#endif

#if NONZERO_CELL_DEBUG
    bool nonzero = (cell.coord.v[0] != 0 || cell.coord.v[1] != 0 || cell.coord.v[2] != 0);
    if (nonzero)
        AFK_DEBUG_PRINTL("testing nonzero shape cell: " << cell)
#endif

    if (!entirelyVisible) visibleCell.testVisibility(
        *camera, someVisible, allVisible);
    if (!someVisible)
    {
#if VISIBLE_CELL_DEBUG
        AFK_DEBUG_PRINTL("cell " << cell << ": visible cell " << visibleCell << ": invisible")
#endif
#if NONZERO_CELL_DEBUG
        if (nonzero)
            AFK_DEBUG_PRINTL("cell " << cell << ": visible cell " << visibleCell << ": invisible")
#endif
        world->shapeCellsInvisible.fetch_add(1);
    }
    else
    {
        bool display = (
            cell.c.coord.v[3] == MIN_CELL_PITCH || visibleCell.testDetailPitch(
                world->averageDetailPitch.get(), *camera, viewerLocation));

        /* Always build the vapour descriptor, because other cells
         * will need it.
         * Only do the render if we actually intend to display the
         * cell, however.
         */
        switch (shape.renderVapourCell(
            threadId,
            entity->getShapeKey(),
            shapeCell,
            display,
            param.shape,
            queue))
        {
        case AFK_Shape::Enqueued:
            if (display)
            {
                /* TODO Produce, and deal with, empties here.  Being able
                 * to track this is an important optimisation, I think.
                 */
                if (shape.generateClaimedShapeCell(
                    visibleCell,
                    shapeCell,
                    claimStatus,
                    threadId,
                    param.shape,
                    queue) == AFK_Shape::NeedsResume)
                {
                    resume = true;
                }
            }
            else
            {
#if VISIBLE_CELL_DEBUG
                AFK_DEBUG_PRINTL("visible cell " << visibleCell << ": without detail pitch " << world->averageDetailPitch.get())
#endif
                /* This cell failed the detail pitch test: recurse through
                 * the subcells
                 */
                recurse = true;
            }
            break;

        case AFK_Shape::NeedsResume:
            resume = true;
            /* Push a resume of this shape cell. */
            break;

        default:
            /* This cell is empty. */
            break;
        }
    }

    if (claimStatus != AFK_CL_TAKEN)
    {
        shapeCell.release(threadId, claimStatus);
    }

    if (resume)
    {
        AFK_WorldWorkQueue::WorkItem resumeItem;
        resumeItem.func = afk_generateShapeCells;
        resumeItem.param = param;
        if (param.shape.dependency) param.shape.dependency->retain();
        queue.push(resumeItem);
        world->shapeCellsResumed.fetch_add(1);
    }

    if (recurse)
    {
        size_t subcellsSize = CUBE(world->sSizes.subdivisionFactor);
        AFK_Cell *subcells = new AFK_Cell[subcellsSize];
        unsigned int subcellsCount = cell.c.subdivide(subcells, subcellsSize, world->sSizes.subdivisionFactor);
        assert(subcellsCount == subcellsSize);

        for (unsigned int i = 0; i < subcellsCount; ++i)
        {
            AFK_WorldWorkQueue::WorkItem subcellItem;
            subcellItem.func                            = afk_generateShapeCells;
            subcellItem.param.shape.cell                = afk_keyedCell(subcells[i], cell.key);
            subcellItem.param.shape.entity              = entity;
            subcellItem.param.shape.world               = world;
            subcellItem.param.shape.viewerLocation      = viewerLocation;
            subcellItem.param.shape.camera              = camera;
            subcellItem.param.shape.flags               = (allVisible ? AFK_SCG_FLAG_ENTIRELY_VISIBLE : 0);
            subcellItem.param.shape.dependency          = NULL;
            queue.push(subcellItem);
        }

        delete[] subcells;
    }

    /* If this cell had a dependency ... */
    if (param.shape.dependency)
    {
        if (param.shape.dependency->check(queue))
        {
            world->dependenciesFollowed.fetch_add(1);
            delete param.shape.dependency;
        }
    }

    return true;
}


/* AFK_Shape implementation */

/* TODO This function is all wrong.  I need to, separately:
 * - Ensure that there's a vapour descriptor (for *every* cell --
 * that bit is correct: I'm always calling this function, but it
 * does too much).
 * - Check whether the vapour cell is within the skeleton, and
 * stop both render and recursing if it isn't
 * - Else, ensure the shape cell has vapour (by enqueueing the
 * render)
 * - And then finally, if the shape cell doesn't have edges,
 * enqueue that.
 */
enum AFK_Shape::CellRenderState AFK_Shape::renderVapourCell(
    unsigned int threadId,
    unsigned int shapeKey,
    AFK_ShapeCell& shapeCell,
    bool doRender,
    const struct AFK_WorldWorkParam::Shape& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_KeyedCell cell                = param.cell;
    AFK_World *world                        = param.world;

    const AFK_ShapeSizes& sSizes            = world->sSizes;
    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;

    unsigned int cubeOffset, cubeCount;
    enum CellRenderState state = NeedsResume;

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, sSizes);
    AFK_VapourCell& vapourCell = (*vapourCellCache)[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);

    if (!doRender || !vapourCell.alreadyEnqueued(cubeOffset, cubeCount))
    {
        AFK_3DList list;

        if (!vapourCell.hasDescriptor())
        {
            if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                claimStatus = vapourCell.upgrade(threadId, claimStatus);

            if (claimStatus == AFK_CL_CLAIMED)
                vapourCell.makeDescriptor(sSizes);
        }

        if (doRender)
        {
            if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                claimStatus = vapourCell.upgrade(threadId, claimStatus);

            if (claimStatus == AFK_CL_CLAIMED)
            {
                if (vapourCell.withinSkeleton())
                {
                    vapourCell.build3DList(threadId, list, sSizes, vapourCellCache);
                    shapeCell.enqueueVapourComputeUnitWithNewVapour(
                        threadId, list, sSizes, vapourJigsaws, world->vapourComputeFair,
                        cubeOffset, cubeCount);
                    vapourCell.enqueued(cubeOffset, cubeCount);
                    state = Enqueued;
                    world->shapeVapoursComputed.fetch_add(1);
                }
                else
                {
                    /* Nothing to see here. */
                    state = Empty;
                }
            }
        }
    }
    else
    {
        shapeCell.enqueueVapourComputeUnitFromExistingVapour(
            threadId, cubeOffset, cubeCount, sSizes, vapourJigsaws, world->vapourComputeFair);
        state = Enqueued;
        world->shapeVapoursComputed.fetch_add(1);
    }

    vapourCell.release(threadId, claimStatus);
    return state;
}

enum AFK_Shape::CellRenderState AFK_Shape::generateClaimedShapeCell(
    const AFK_VisibleCell& visibleCell,
    AFK_ShapeCell& shapeCell,
    enum AFK_ClaimStatus& claimStatus,
    unsigned int threadId,
    const struct AFK_WorldWorkParam::Shape& param,
    AFK_WorldWorkQueue& queue)
{
    AFK_Entity *entity                      = param.entity;
    AFK_World *world                        = param.world;

    Mat4<float> worldTransform              = entity->obj.getTransformation();

    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws       = world->edgeJigsaws;

    enum CellRenderState state = NeedsResume;

    /* Try to get this shape cell set up and enqueued. */
    if (!shapeCell.hasEdges(edgeJigsaws))
    {
        /* I need to generate stuff for this cell -- which means I need
         * to upgrade my claim.
         */
        if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
            claimStatus = shapeCell.upgrade(threadId, claimStatus);

        if (claimStatus == AFK_CL_CLAIMED)
        {
            /* The vapour must be there already. */
            assert(shapeCell.hasVapour(vapourJigsaws));
            shapeCell.enqueueEdgeComputeUnit(
                threadId, shapeCellCache, vapourJigsaws, edgeJigsaws, world->edgeComputeFair);
            world->shapeEdgesComputed.fetch_add(1);
        }
    }

    if (shapeCell.hasEdges(edgeJigsaws))
    {
        shapeCell.enqueueEdgeDisplayUnit(
            worldTransform,
            edgeJigsaws,
            world->entityDisplayFair);
        state = Enqueued;
    }

    return state;
}

AFK_Shape::AFK_Shape(
    const AFK_Config *config,
    unsigned int shapeCacheSize)
{
    /* This is naughty, but I really want an auto-create
     * here.
     */
    unsigned int shapeCellCacheEntries = shapeCacheSize /
        (2 * config->shape_skeletonMaxSize * 6 * SQUARE(config->shape_pointSubdivisionFactor));
    unsigned int shapeCellCacheBitness = afk_suggestCacheBitness(shapeCellCacheEntries);
    shapeCellCache = new AFK_SHAPE_CELL_CACHE(
        shapeCellCacheBitness,
        4,
        AFK_HashKeyedCell(),
        shapeCellCacheEntries / 2,
        0xfffffffdu);

    unsigned int vapourCellCacheEntries = shapeCacheSize /
        (2 * config->shape_skeletonMaxSize * CUBE(config->shape_pointSubdivisionFactor));
    unsigned int vapourCellCacheBitness = afk_suggestCacheBitness(vapourCellCacheEntries);
    vapourCellCache = new AFK_VAPOUR_CELL_CACHE(
        vapourCellCacheBitness,
        4,
        AFK_HashKeyedCell(),
        vapourCellCacheEntries / 2,
        0xfffffffcu);
}

AFK_Shape::~AFK_Shape()
{
    delete vapourCellCache;
    delete shapeCellCache;
}

void AFK_Shape::updateWorld(void)
{
    shapeCellCache->doEvictionIfNecessary();
    vapourCellCache->doEvictionIfNecessary();
}

void AFK_Shape::printCacheStats(std::ostream& os, const std::string& prefix)
{
    shapeCellCache->printStats(os, "Shape cell cache");
    vapourCellCache->printStats(os, "Vapour cell cache");
}

