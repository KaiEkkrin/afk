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

/* The vapour descriptor-only worker */

bool afk_generateVapourDescriptor(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_Cell cell                     = param.shape.cell;
    AFK_Entity *entity                      = param.shape.entity;
    AFK_World *world                        = param.shape.world;

    AFK_Shape *shape                        = entity->shape;

    AFK_Cell vc = afk_vapourCell(cell);
    AFK_VapourCell& vapourCell = (*(shape->vapourCellCache))[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

    /* I need an exclusive claim for this operation. */
    if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
    {
        claimStatus = vapourCell.upgrade(threadId, claimStatus);
    }

    if (claimStatus == AFK_CL_CLAIMED)
    {
        if (!vapourCell.hasDescriptor())
        {
            vapourCell.makeDescriptor(entity->shapeKey, world->sSizes);
            world->separateVapoursComputed.fetch_add(1);
        }
    }
    else
    {
        /* Oh dear!  I'd better resume the damn thing */
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
    const AFK_Cell cell                     = param.shape.cell;
    AFK_Entity *entity                      = param.shape.entity;
    AFK_World *world                        = param.shape.world;
    const Vec3<float>& viewerLocation       = param.shape.viewerLocation;
    const AFK_Camera *camera                = param.shape.camera;

    bool entirelyVisible                    = ((param.shape.flags & AFK_SCG_FLAG_ENTIRELY_VISIBLE) != 0);

    AFK_Shape *shape                        = entity->shape;
    Mat4<float> worldTransform              = entity->obj.getTransformation();

    AFK_ShapeCell& shapeCell = (*(shape->shapeCellCache))[cell];
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
        if (cell.coord.v[3] == MIN_CELL_PITCH || visibleCell.testDetailPitch(
            world->averageDetailPitch.get(), *camera, viewerLocation))
        {
            shape->generateClaimedShapeCell(
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
            unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize, world->sSizes.subdivisionFactor);

            if (subcellsCount == subcellsSize)
            {
                for (unsigned int i = 0; i < subcellsCount; ++i)
                {
                    AFK_WorldWorkQueue::WorkItem subcellItem;
                    subcellItem.func                            = afk_generateShapeCells;
                    subcellItem.param.shape.cell                = subcells[i];
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

bool AFK_Shape::enqueueVapourCell(
    unsigned int threadId,
    unsigned int shapeKey,
    AFK_ShapeCell& shapeCell,
    const struct AFK_WorldWorkParam::Shape& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_Cell cell                     = param.cell;
    AFK_World *world                        = param.world;

    const AFK_ShapeSizes& sSizes            = world->sSizes;
    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;

    unsigned int cubeOffset, cubeCount;
    bool success = false;

    AFK_Cell vc = afk_vapourCell(cell);
    AFK_VapourCell& vapourCell = (*vapourCellCache)[vc];
    vapourCell.bind(vc);
    AFK_ClaimStatus claimStatus = vapourCell.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

    if (!vapourCell.alreadyEnqueued(cubeOffset, cubeCount))
    {
        AFK_3DList list;
        std::vector<AFK_Cell> missingCells;

        /* I need to upgrade my claim to generate the descriptor. */
        if (claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
        {
            claimStatus = vapourCell.upgrade(threadId, claimStatus);
        }

        if (claimStatus == AFK_CL_CLAIMED)
        {
            if (!vapourCell.hasDescriptor())
                vapourCell.makeDescriptor(shapeKey, sSizes);

            vapourCell.build3DList(threadId, list, missingCells, sSizes.subdivisionFactor, vapourCellCache);
            if (missingCells.size() > 0)
            {
                /* Enqueue these cells for vapour render and a
                 * dependent resume
                 */
                AFK_WorldWorkQueue::WorkItem finalResumeItem;
                finalResumeItem.func = afk_generateShapeCells;
                finalResumeItem.param.shape = param;
                if (param.dependency) param.dependency->retain();
                AFK_WorldWorkParam::Dependency *dependency = new AFK_WorldWorkParam::Dependency(finalResumeItem);

                for (std::vector<AFK_Cell>::iterator mcIt = missingCells.begin();
                    mcIt != missingCells.end(); ++mcIt)
                {
                    AFK_WorldWorkQueue::WorkItem missingCellItem;
                    missingCellItem.func = afk_generateVapourDescriptor;
                    missingCellItem.param.shape = param;
                    missingCellItem.param.shape.cell = *mcIt;
                    missingCellItem.param.shape.dependency = dependency;

                    dependency->retain();
                    queue.push(missingCellItem);
                }
            }
            else
            {
                shapeCell.enqueueVapourComputeUnitWithNewVapour(
                    threadId, list, sSizes, vapourJigsaws, world->vapourComputeFair,
                    cubeOffset, cubeCount);
                vapourCell.enqueued(cubeOffset, cubeCount);
            }

            success = true; /* I already dealt with the potential resume above */
        }
    }
    else
    {
        if (claimStatus == AFK_CL_CLAIMED_SHARED ||
            claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
        {
            shapeCell.enqueueVapourComputeUnitFromExistingVapour(
                threadId, cubeOffset, cubeCount, sSizes, vapourJigsaws, world->vapourComputeFair);
            success = true;
        }
    }

    if (claimStatus == AFK_CL_CLAIMED ||
        claimStatus == AFK_CL_CLAIMED_SHARED ||
        claimStatus == AFK_CL_CLAIMED_UPGRADABLE)
    {
        vapourCell.release(threadId, claimStatus);
    }

    return success;
}

void AFK_Shape::generateClaimedShapeCell(
    AFK_ShapeCell& shapeCell,
    enum AFK_ClaimStatus& claimStatus,
    unsigned int threadId,
    const struct AFK_WorldWorkParam::Shape& param,
    AFK_WorldWorkQueue& queue)
{
    AFK_Entity *entity                      = param.entity;
    AFK_World *world                        = param.world;

    AFK_Shape *shape                        = entity->shape;
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
                if (shape->enqueueVapourCell(threadId, entity->shapeKey, shapeCell,
                    param, queue))
                {
                    world->shapeVapoursComputed.fetch_add(1);
                }
                else
                {
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
        shapeCell.enqueueEdgeDisplayUnit(
            worldTransform,
            edgeJigsaws,
            world->entityDisplayFair);
    }
}

AFK_Shape::AFK_Shape():
    AFK_Claimable()
{
    /* This is naughty, but I really want an auto-create
     * here.
     */
    unsigned int shapeCellCacheBitness = afk_suggestCacheBitness(
        afk_core.config->shape_skeletonMaxSize * 4);
    shapeCellCache = new AFK_SHAPE_CELL_CACHE(
        shapeCellCacheBitness,
        4,
        AFK_HashCell());

    unsigned int vapourCellCacheBitness = afk_suggestCacheBitness(
        afk_core.config->shape_skeletonMaxSize);
    vapourCellCache = new AFK_VAPOUR_CELL_CACHE(
        vapourCellCacheBitness,
        4,
        AFK_HashCell());
}

AFK_Shape::~AFK_Shape()
{
    delete vapourCellCache;
    delete shapeCellCache;
}

AFK_Frame AFK_Shape::getCurrentFrame(void) const
{
    return afk_core.computingFrame;
}

bool AFK_Shape::canBeEvicted(void) const
{
    /* This is a tweakable value ... */
    bool canEvict = ((afk_core.computingFrame - lastSeen) > 10);
    return canEvict;
}

std::ostream& operator<<(std::ostream& os, const AFK_Shape& shape)
{
    os << "Shape";
    return os;
}

