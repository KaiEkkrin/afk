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
            /* TODO I think I've actually forgotten to do the
             * equivalent operation with finer-LoD vapour cells,
             * so the embellish() function of the skeleton can't
             * really do anything useful right now (apart from
             * make it vanish, I guess).
             * Fix this.
             */
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

#if VISIBLE_CELL_DEBUG
#define DEBUG_VISIBLE_CELL(message) AFK_DEBUG_PRINTL("cell " << cell << ": visible cell " << visibleCell << ": " << message)
#else
#define DEBUG_VISIBLE_CELL(message)
#endif

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
    AFK_ClaimStatus shapeCellClaimStatus = shapeCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);

    bool resume = false;

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;
    AFK_VisibleCell visibleCell;
    visibleCell.bindToCell(cell, SHAPE_CELL_WORLD_SCALE, worldTransform);

    if (!entirelyVisible) visibleCell.testVisibility(
        *camera, someVisible, allVisible);
    if (!someVisible)
    {
        DEBUG_VISIBLE_CELL("invisible")
        world->shapeCellsInvisible.fetch_add(1);
    }
    else
    {
        /* TODO Artificial min cell pitch here to check for
         * cross-LoD overlaps.
         */
        bool display = (
            cell.c.coord.v[3] == 1024 || visibleCell.testDetailPitch(
                world->getEntityDetailPitch(), *camera, viewerLocation));

        /* Always build the vapour descriptor, because other cells
         * will need it.
         * Only do the render if we actually intend to display the
         * cell, however.
         */
        AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
        AFK_VapourCell& vapourCell = (*(shape.vapourCellCache))[vc];
        vapourCell.bind(vc);
        AFK_ClaimStatus vapourCellClaimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);

        if (!vapourCell.hasDescriptor())
        {
            if (vapourCellClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                vapourCellClaimStatus = vapourCell.upgrade(threadId, vapourCellClaimStatus);

            if (vapourCellClaimStatus == AFK_CL_CLAIMED)
            {
                /* This is a lower level vapour cell (the top level ones were
                 * made in afk_generateEntity() ) and is dependent on its
                 * parent vapour cell's descriptor
                 */
                AFK_KeyedCell upperVC = vc.parent(world->sSizes.subdivisionFactor);
                AFK_VapourCell& upperVapourCell = shape.vapourCellCache->at(upperVC);
                AFK_ClaimStatus upperVCClaimStatus = upperVapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);
                vapourCell.makeDescriptor(upperVapourCell, world->sSizes);
                upperVapourCell.release(threadId, upperVCClaimStatus);
            }
        }

        if (vapourCell.hasDescriptor())
        {
            /* TODO: I can't cull inner cells by solidity here, but instead
             * need to rely on feedback from the edge compute step.
             * Arrange that, and update vapour cells with whether or not
             * they're solid (a bit like the y reduce feedback), so that I
             * can decide whether or not to recurse into finer detail vapour
             * cells based on whether they're surrounded by solid cells or
             * not.
             */
            if (vapourCell.withinSkeleton(cell, world->sSizes))
            {
                if (display) 
                {
                    if (!shape.generateClaimedShapeCell(
                        threadId, vapourCell, shapeCell, vapourCellClaimStatus, shapeCellClaimStatus, worldTransform, world))
                    {
                        DEBUG_VISIBLE_CELL("needs resume")
                        resume = true;
                    }
                    else
                    {
                        DEBUG_VISIBLE_CELL("generated")
                    }
                }
                else
                {
                    DEBUG_VISIBLE_CELL("recursing into subcells")

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
            }
            else
            {
                DEBUG_VISIBLE_CELL("outside skeleton")
            }
        }
        else
        {
            DEBUG_VISIBLE_CELL("needs resume at top level")
            resume = true;
        }

        if (vapourCellClaimStatus != AFK_CL_TAKEN)
        {
            vapourCell.release(threadId, vapourCellClaimStatus);
        }
    }

    if (shapeCellClaimStatus != AFK_CL_TAKEN)
    {
        shapeCell.release(threadId, shapeCellClaimStatus);
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

bool AFK_Shape::generateClaimedShapeCell(
    unsigned int threadId,
    AFK_VapourCell& vapourCell,
    AFK_ShapeCell& shapeCell,
    enum AFK_ClaimStatus& vapourCellClaimStatus,
    enum AFK_ClaimStatus& shapeCellClaimStatus,
    const Mat4<float>& worldTransform,
    AFK_World *world)
{
    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws       = world->edgeJigsaws;

    bool success = false;

    /* Try to get this shape cell set up and enqueued. */
    if (!shapeCell.hasEdges(edgeJigsaws))
    {
        /* I need to generate stuff for this cell -- which means I need
         * to upgrade my claim.
         */
        if (shapeCellClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
            shapeCellClaimStatus = shapeCell.upgrade(threadId, shapeCellClaimStatus);

        if (shapeCellClaimStatus == AFK_CL_CLAIMED)
        {
            /* The vapour descriptor must be there already. */
            assert(vapourCell.hasDescriptor());

            /* Check whether we have rendered vapour here.  If we don't,
             * we need to make that first.
             */
            if (!shapeCell.hasVapour(vapourJigsaws))
            {
                /* Have we already enqueued a different part of this vapour
                 * cell for compute?
                 */
                unsigned int cubeOffset, cubeCount;
                if (vapourCell.alreadyEnqueued(cubeOffset, cubeCount))
                {
                    int adjacency = vapourCell.skeletonFullAdjacency(shapeCell.getCell(), world->sSizes);
                    shapeCell.enqueueVapourComputeUnitFromExistingVapour(
                        threadId, adjacency, cubeOffset, cubeCount, world->sSizes, vapourJigsaws, world->vapourComputeFair);
                    world->shapeVapoursComputed.fetch_add(1);
                }
                else
                {
                    /* I need to upgrade my vapour cell claim first */
                    if (vapourCellClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                        vapourCellClaimStatus = vapourCell.upgrade(threadId, vapourCellClaimStatus);

                    if (vapourCellClaimStatus == AFK_CL_CLAIMED)
                    {
                        AFK_3DList list;
                        vapourCell.build3DList(threadId, list, world->sSizes, vapourCellCache);

                        int adjacency = vapourCell.skeletonFullAdjacency(shapeCell.getCell(), world->sSizes);
                        shapeCell.enqueueVapourComputeUnitWithNewVapour(
                            threadId, adjacency, list, world->sSizes, vapourJigsaws, world->vapourComputeFair, cubeOffset, cubeCount);
                        vapourCell.enqueued(cubeOffset, cubeCount);
                        world->shapeVapoursComputed.fetch_add(1);
                    }
                }
            }

            if (shapeCell.hasVapour(vapourJigsaws))
            {
                shapeCell.enqueueEdgeComputeUnit(
                    threadId, shapeCellCache, vapourJigsaws, edgeJigsaws, world->edgeComputeFair);
                world->shapeEdgesComputed.fetch_add(1);
            }
        }
    }

    if (shapeCell.hasEdges(edgeJigsaws))
    {
        shapeCell.enqueueEdgeDisplayUnit(
            worldTransform,
            edgeJigsaws,
            world->entityDisplayFair);
        success = true;
    }

    return success;
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

