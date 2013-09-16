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

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
    AFK_VapourCell& vapourCell = (*(shape.vapourCellCache))[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

    if (!vapourCell.hasDescriptor())
    {
        /* Get an exclusive claim, and make its descriptor. */
        if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
        {
            claimStatus = vapourCell.upgrade(threadId, claimStatus);
        }

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
            AFK_WorldWorkQueue::WorkItem resumeItem;
            resumeItem.func = afk_generateEntity;
            resumeItem.param = param;
            if (param.shape.dependency) param.shape.dependency->retain();
            queue.push(resumeItem);
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

    if (claimStatus == AFK_CL_CLAIMED ||
        claimStatus == AFK_CL_CLAIMED_UPGRADABLE ||
        claimStatus == AFK_CL_CLAIMED_SHARED)
    {
        vapourCell.release(threadId, claimStatus);
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

/* The vapour descriptor-only worker */

bool afk_generateVapourDescriptor(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_KeyedCell cell                = param.shape.cell;
    AFK_World *world                        = param.shape.world;

    AFK_Shape& shape                        = world->shape;

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
    AFK_VapourCell& vapourCell = (*(shape.vapourCellCache))[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

    /* I need an exclusive claim for this operation. */
    if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
    {
        claimStatus = vapourCell.upgrade(threadId, claimStatus);
    }

    bool needsResume = true;

    if (claimStatus == AFK_CL_CLAIMED)
    {
        if (!vapourCell.hasDescriptor())
        {
            /* We shouldn't get a kersplode here, so long as this
             * task is enqueued as a dependent task of the tasks
             * that generate higher-level vapour.
             */
            AFK_KeyedCell parentVC = vc.parent(world->sSizes.subdivisionFactor);
            //try
            {
                AFK_VapourCell& upperCell = shape.vapourCellCache->at(parentVC);
                AFK_ClaimStatus upperClaimStatus = upperCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
                if (upperClaimStatus == AFK_CL_CLAIMED_SHARED ||
                    upperClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                {
                    vapourCell.makeDescriptor(upperCell, world->sSizes);
                    world->separateVapoursComputed.fetch_add(1);
                    upperCell.release(threadId, upperClaimStatus);
                    needsResume = false;
                }
            }
            //catch (AFK_PolymerOutOfRange)
            {
                /* I should be okay to just let this resume...
                 * If I find myself entering infinite loops, it's
                 * probably because the logic that enqueues vapour
                 * descriptors is wrong and the one this cell wants
                 * is never enqueued.
                 * -- okay, confirmed that this hangs...  there's
                 * something up with that logic, check for
                 * shape cell/vapour cell correspondance.
                 */
            }
        }
    }

    if (needsResume)
    {
        /* Oh dear!  Try again later; the rest of my logic should
         * ensure that it'll pop up
         */
        AFK_WorldWorkQueue::WorkItem resumeItem;
        resumeItem.func = afk_generateVapourDescriptor;
        resumeItem.param = param;
        if (param.shape.dependency) param.shape.dependency->retain();
        queue.push(resumeItem);
    }

    if (claimStatus == AFK_CL_CLAIMED ||
        claimStatus == AFK_CL_CLAIMED_UPGRADABLE ||
        claimStatus == AFK_CL_CLAIMED_SHARED)
    {
        vapourCell.release(threadId, claimStatus);
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

    AFK_ShapeCell& shapeCell = (*(shape.shapeCellCache))[cell];
    shapeCell.bind(cell);
    AFK_ClaimStatus claimStatus = shapeCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

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
        if (cell.c.coord.v[3] == MIN_CELL_PITCH || visibleCell.testDetailPitch(
            world->averageDetailPitch.get(), *camera, viewerLocation))
        {
            shape.generateClaimedShapeCell(
                visibleCell,
                shapeCell,
                claimStatus,
                threadId,
                param.shape,
                queue);
        }
        else
        {
#if VISIBLE_CELL_DEBUG
            AFK_DEBUG_PRINTL("visible cell " << visibleCell << ": without detail pitch " << world->averageDetailPitch.get())
#endif

            /* This cell failed the detail pitch test: recurse through
             * the subcells
             */
            size_t subcellsSize = CUBE(world->sSizes.subdivisionFactor);
            AFK_Cell *subcells = new AFK_Cell[subcellsSize];
            unsigned int subcellsCount = cell.c.subdivide(subcells, subcellsSize, world->sSizes.subdivisionFactor);

            if (subcellsCount == subcellsSize)
            {
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
            }
            else
            {
                std::ostringstream ss;
                ss << "Cell " << cell << " subdivided into " << subcellsCount << " not " << subcellsSize;
                throw AFK_Exception(ss.str());
            }

            delete[] subcells;
        }
    }

    if (claimStatus == AFK_CL_CLAIMED ||
        claimStatus == AFK_CL_CLAIMED_UPGRADABLE ||
        claimStatus == AFK_CL_CLAIMED_SHARED)
    {
        shapeCell.release(threadId, claimStatus);
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

enum AFK_Shape::VapourCellState AFK_Shape::enqueueVapourCell(
    unsigned int threadId,
    unsigned int shapeKey,
    AFK_ShapeCell& shapeCell,
    const struct AFK_WorldWorkParam::Shape& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_KeyedCell cell                = param.cell;
    AFK_World *world                        = param.world;

    const AFK_ShapeSizes& sSizes            = world->sSizes;
    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;

    unsigned int cubeOffset, cubeCount;
    enum VapourCellState state = NeedsResume;

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, sSizes);
    AFK_VapourCell& vapourCell = (*vapourCellCache)[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

    /* TODO Here, I need to cope with empty cells, which _will_
     * happen.
     */
    if (!vapourCell.alreadyEnqueued(cubeOffset, cubeCount))
    {
        AFK_3DList list;
        std::vector<AFK_KeyedCell> missingCells;

        /* I need to upgrade my claim to generate the descriptor. */
        if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
        {
            claimStatus = vapourCell.upgrade(threadId, claimStatus);
        }

        if (claimStatus == AFK_CL_CLAIMED)
        {
            if (!vapourCell.hasDescriptor())
                vapourCell.makeDescriptor(sSizes);

            if (vapourCell.withinSkeleton())
            {
                vapourCell.build3DList(threadId, list, missingCells, sSizes, vapourCellCache);
                if (missingCells.size() > 0)
                {
                    /* Enqueue the missing cells for vapour render.  Note
                     * that each one is dependent on the coarser LoD one
                     * that follows it, so what I have is a dependency
                     * chain (I hope this works):
                     */
                    AFK_WorldWorkQueue::WorkItem finalResumeItem;
                    finalResumeItem.func = afk_generateShapeCells;
                    finalResumeItem.param.shape = param;
                    if (param.dependency) param.dependency->retain();
                    AFK_WorldWorkParam::Dependency *lastDependency = new AFK_WorldWorkParam::Dependency(finalResumeItem);

                    AFK_WorldWorkQueue::WorkItem missingCellItem;

                    for (std::vector<AFK_KeyedCell>::iterator mcIt = missingCells.begin();
                        mcIt != missingCells.end(); ++mcIt)
                    {
                        /* Sanity check */
                        if (mcIt->c.coord.v[3] >= SHAPE_CELL_MAX_DISTANCE)
                            throw AFK_Exception("Missing the top vapour cell");

                        missingCellItem.func = afk_generateVapourDescriptor;
                        missingCellItem.param.shape = param;
                        missingCellItem.param.shape.cell = *mcIt;
                        missingCellItem.param.shape.dependency = lastDependency;
                        missingCellItem.param.shape.dependency->retain();

                        lastDependency = new AFK_WorldWorkParam::Dependency(missingCellItem);
                    }

                    queue.push(missingCellItem);
                    delete lastDependency;
                }
                else
                {
                    shapeCell.enqueueVapourComputeUnitWithNewVapour(
                        threadId, list, sSizes, vapourJigsaws, world->vapourComputeFair,
                        cubeOffset, cubeCount);
                    vapourCell.enqueued(cubeOffset, cubeCount);
                }

                state = Enqueued; /* I already dealt with the potential resume above */
            }
            else
            {
                /* Nothing to see here */
                state = Empty;
            }
        }
    }
    else
    {
        if (claimStatus == AFK_CL_CLAIMED_SHARED ||
            claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
        {
            shapeCell.enqueueVapourComputeUnitFromExistingVapour(
                threadId, cubeOffset, cubeCount, sSizes, vapourJigsaws, world->vapourComputeFair);
            state = Enqueued;
        }
    }

    if (claimStatus == AFK_CL_CLAIMED ||
        claimStatus == AFK_CL_CLAIMED_SHARED ||
        claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
    {
        vapourCell.release(threadId, claimStatus);
    }

    return state;
}

void AFK_Shape::generateClaimedShapeCell(
    const AFK_VisibleCell& visibleCell,
    AFK_ShapeCell& shapeCell,
    enum AFK_ClaimStatus& claimStatus,
    unsigned int threadId,
    const struct AFK_WorldWorkParam::Shape& param,
    AFK_WorldWorkQueue& queue)
{
    AFK_Entity *entity                      = param.entity;
    AFK_World *world                        = param.world;

    AFK_Shape& shape                        = world->shape;
    Mat4<float> worldTransform              = entity->obj.getTransformation();

    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws       = world->edgeJigsaws;

    /* Try to get this shape cell set up and enqueued. */
    if (!shapeCell.hasEdges(edgeJigsaws))
    {
        /* I need to generate stuff for this cell -- which means I need
         * to upgrade my claim.
         */
        if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
        {
            claimStatus = shapeCell.upgrade(threadId, claimStatus);
        }

        switch (claimStatus)
        {
        case AFK_CL_CLAIMED:
            if (!shapeCell.hasVapour(vapourJigsaws))
            {
                /* I need to generate the vapour too. */
                switch (shape.enqueueVapourCell(
                    threadId, entity->shapeKey, shapeCell, param, queue))
                {
                case AFK_Shape::Enqueued:
                    world->shapeVapoursComputed.fetch_add(1);
                    break;

                case AFK_Shape::NeedsResume:
                    /* Enqueue a resume of this one and drop out. */
                    AFK_WorldWorkQueue::WorkItem resumeItem;
                    resumeItem.func = afk_generateShapeCells;
                    resumeItem.param.shape = param;

                    if (param.dependency) param.dependency->retain();
                    queue.push(resumeItem);
                    world->shapeCellsResumed.fetch_add(1);

                    return;

                default:
                    /* This cell is empty, drop out. */
                    return;
                }
            }

            if (!shapeCell.hasEdges(edgeJigsaws))
            {
                shapeCell.enqueueEdgeComputeUnit(
                    threadId, vapourJigsaws, edgeJigsaws, world->edgeComputeFair);
                world->shapeEdgesComputed.fetch_add(1);
            }
            break;

        default:
            /* Enqueue a resume of this one and drop out. */
            AFK_WorldWorkQueue::WorkItem resumeItem;
            resumeItem.func = afk_generateShapeCells;
            resumeItem.param.shape = param;

            if (param.dependency) param.dependency->retain();
            queue.push(resumeItem);
            world->shapeCellsResumed.fetch_add(1);

            return;
        }
    }

    if (claimStatus == AFK_CL_CLAIMED ||
        claimStatus == AFK_CL_CLAIMED_UPGRADABLE ||
        claimStatus == AFK_CL_CLAIMED_SHARED)
    {
        /* TODO remove debug */
        //AFK_DEBUG_PRINTL("At visible cell " << visibleCell << ": enqueueing shape cell for display: " << shapeCell.getCell())
        shapeCell.enqueueEdgeDisplayUnit(
            worldTransform,
            edgeJigsaws,
            world->entityDisplayFair);
    }
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

