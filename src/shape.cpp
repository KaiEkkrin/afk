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

#include "afk.hpp"

#include <cmath>
#include <cstring>

#include "camera.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"
#include "shape.hpp"
#include "vapour_cell.hpp"
#include "world.hpp"


/* Shape worker flags. */
#define AFK_SCG_FLAG_ENTIRELY_VISIBLE       1

/* The top-level entity worker. */
bool afk_generateEntity(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    const struct AFK_WorldWorkThreadLocal& threadLocal,
    AFK_WorldWorkQueue& queue)
{
    AFK_KeyedCell cell                      = param.shape.cell;
    AFK_World *world                        = afk_core.world;

    AFK_Shape& shape                        = world->shape;

    bool needsResume = false;

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
    auto claim = shape.vapourCellCache->insertAndClaim(threadId, vc, AFK_CL_BLOCK | AFK_CL_UPGRADE);
    if (claim.isValid())
    {    
        if (!claim.getShared().hasDescriptor())
        {
            /* Get an exclusive claim, and make its descriptor. */
            if (claim.upgrade())
            {
                AFK_VapourCell& vapourCell = claim.get();
        
                if (!vapourCell.hasDescriptor())
                {
                    vapourCell.makeDescriptor(vc, world->sSizes);
                    world->separateVapoursComputed.fetch_add(1);
                }
            }
        }
        
        if (claim.getShared().hasDescriptor())
        {
            /* Go through the skeleton... */
            AFK_VapourCell::ShapeCells shapeCells(vc, claim.getShared(), world->sSizes);
            while (shapeCells.hasNext())
            {
                AFK_KeyedCell nextCell = shapeCells.next();

                /* Track the volume I'm enumerating here */
                world->volumeLeftToEnumerate.fetch_add(CUBE(nextCell.c.coord.v[3]));
        
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
                shapeCellItem.param.shape.dependency = nullptr;
                queue.push(shapeCellItem);
            }
        }
        else
        {
            /* I'd better resume this later. */
            needsResume = true;
        }
    }
    else
    {
        needsResume = true;
    }

    if (needsResume)
    {
        /* I'm about to want to enumerate this volume again */
        world->volumeLeftToEnumerate.fetch_add(CUBE(cell.c.coord.v[3]));

        AFK_WorldWorkQueue::WorkItem resumeItem;
        resumeItem.func = afk_generateEntity;
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

    /* I've finished with this cell */
    world->volumeLeftToEnumerate.fetch_sub(CUBE(cell.c.coord.v[3]));

    return true;
}

/* The shape worker */

#define VISIBLE_CELL_DEBUG 1

#if VISIBLE_CELL_DEBUG
#define DEBUG_VISIBLE_CELL(message) AFK_DEBUG_PRINTL("cell " << cell << ": visible cell " << visibleCell << ": " << message)
#else
#define DEBUG_VISIBLE_CELL(message)
#endif

bool afk_generateShapeCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    const struct AFK_WorldWorkThreadLocal& threadLocal,
    AFK_WorldWorkQueue& queue)
{
    const AFK_KeyedCell cell                = param.shape.cell;
    Mat4<float> worldTransform              = param.shape.transformation;
    const AFK_Camera& camera                = threadLocal.camera;
    const Vec3<float>& viewerLocation       = threadLocal.viewerLocation;

    AFK_World *world                        = afk_core.world;

    bool entirelyVisible                    = ((param.shape.flags & AFK_SCG_FLAG_ENTIRELY_VISIBLE) != 0);

    AFK_Shape& shape                        = world->shape;

    bool needsResume = false;

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;
    AFK_VisibleCell visibleCell;
    visibleCell.bindToCell(cell, SHAPE_CELL_WORLD_SCALE, worldTransform);

    if (!entirelyVisible) visibleCell.testVisibility(
        camera, someVisible, allVisible);
    if (!someVisible)
    {
#if AFK_SHAPE_ENUM_DEBUG
        AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << " of entity: worldCell=" << param.shape.asedWorldCell << ", entity counter=" << param.shape.asedCounter << " invisible")
#endif

        DEBUG_VISIBLE_CELL("invisible")
        world->shapeCellsInvisible.fetch_add(1);
    }
    else
    {
        bool display = (
            cell.c.coord.v[3] == 1 || visibleCell.testDetailPitch(
                world->getEntityDetailPitch(threadLocal.detailPitch), camera, viewerLocation));

        /* Always build the vapour descriptor, because other cells
         * will need it.
         * Only do the render if we actually intend to display the
         * cell, however.
         */
        AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
        auto vapourCellClaim = shape.vapourCellCache->insertAndClaim(threadId, vc, AFK_CL_BLOCK | AFK_CL_UPGRADE);
        if (vapourCellClaim.isValid())
        {
            const AFK_VapourCell& vapourCell = vapourCellClaim.getShared();
     
            if (!vapourCell.hasDescriptor())
            {
                if (vapourCellClaim.upgrade())
                {
                    /* This is a lower level vapour cell (the top level ones were
                     * made in afk_generateEntity() ) and is dependent on its
                     * parent vapour cell's descriptor
                     */
                    AFK_KeyedCell upperVC = vc.parent(world->sSizes.subdivisionFactor);
                    auto upperVapourCellClaim =
                        shape.vapourCellCache->getAndClaim(threadId, upperVC, AFK_CL_BLOCK | AFK_CL_SHARED);
                    if (upperVapourCellClaim.isValid())
                        vapourCellClaim.get().makeDescriptor(vc, upperVC, upperVapourCellClaim.getShared(), world->sSizes);
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
                if (vapourCell.withinSkeleton(vc, cell, world->sSizes))
                {
                    /* I want that shape cell now ... */
                    auto shapeCellClaim = shape.shapeCellCache->insertAndClaim(threadId, cell, AFK_CL_BLOCK | AFK_CL_UPGRADE);
                    if (shapeCellClaim.isValid())
                    {
                        if (shapeCellClaim.getShared().getDMin() < 0.0f &&
                            shapeCellClaim.getShared().getDMax() > 0.0f)
                        {
                            /* There is something interesting here -- it's not empty
                             * or solid
                             */
                            if (display) 
                            {
                                if (!shape.generateClaimedShapeCell(
                                    threadId, vc, cell, vapourCellClaim, shapeCellClaim, worldTransform))
                                {
                                    DEBUG_VISIBLE_CELL("needs resume")
                                    needsResume = true;
                                }
                                else
                                {
                                    DEBUG_VISIBLE_CELL("generated")
#if AFK_SHAPE_ENUM_DEBUG
                                    AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << " of entity: worldCell=" << param.shape.asedWorldCell << ", entity counter=" << param.shape.asedCounter << " generated")
#endif
                                }
                            }
                            else
                            {
                                DEBUG_VISIBLE_CELL("recursing into subcells")
        
                                /* I can drop this before adding to the queue */
                                shapeCellClaim.release();
            
                                /* I'm about to enumerate this cell's volume in subcells */
                                world->volumeLeftToEnumerate.fetch_add(CUBE(cell.c.coord.v[3]));
             
                                size_t subcellsSize = CUBE(world->sSizes.subdivisionFactor);
                                AFK_Cell *subcells = new AFK_Cell[subcellsSize];
                                unsigned int subcellsCount = cell.c.subdivide(subcells, subcellsSize, world->sSizes.subdivisionFactor);
                                assert(subcellsCount == subcellsSize);
             
                                for (unsigned int i = 0; i < subcellsCount; ++i)
                                {
                                    AFK_WorldWorkQueue::WorkItem subcellItem;
                                    subcellItem.func                            = afk_generateShapeCells;
                                    subcellItem.param                           = param;
                                    subcellItem.param.shape.cell                = afk_keyedCell(subcells[i], cell.key);
                                    subcellItem.param.shape.flags               = (allVisible ? AFK_SCG_FLAG_ENTIRELY_VISIBLE : 0);
                                    subcellItem.param.shape.dependency          = nullptr;
                                    queue.push(subcellItem);
             
#if AFK_SHAPE_ENUM_DEBUG
                                    AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << " of entity: worldCell=" << param.shape.asedWorldCell << ", entity counter=" << param.shape.asedCounter << " recursed")
#endif
                                }
                        
                                delete[] subcells;
                            }
                        }
                        else
                        {
                            DEBUG_VISIBLE_CELL("empty or solid")
                            world->shapeCellsReducedOut.fetch_add(1);
                        }
                    }
                    else
                    {
                        DEBUG_VISIBLE_CELL("can't claim shape cell")
                        needsResume = true;
                    }
                }
                else
                {
                    DEBUG_VISIBLE_CELL("outside skeleton")
#if AFK_SHAPE_ENUM_DEBUG
                    AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << " of entity: worldCell=" << param.shape.asedWorldCell << ", entity counter=" << param.shape.asedCounter << " outsideskeleton")
#endif
                }
            }
            else
            {
                DEBUG_VISIBLE_CELL("needs resume at top level")
                needsResume = true;
            }
        }
        else
        {
            needsResume = true;
        }
    }

    if (needsResume)
    {
        /* Add the resume volume to the amount left */
        world->volumeLeftToEnumerate.fetch_add(CUBE(cell.c.coord.v[3]));

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

    /* I have finished this cell and can check its volume off */
    world->volumeLeftToEnumerate.fetch_sub(CUBE(cell.c.coord.v[3]));

    return true;
}


/* AFK_Shape implementation */

bool AFK_Shape::generateClaimedShapeCell(
    unsigned int threadId,
    const AFK_KeyedCell& vc,
    const AFK_KeyedCell& cell,
    AFK_VAPOUR_CELL_CACHE::Claim& vapourCellClaim,
    AFK_SHAPE_CELL_CACHE::Claim& shapeCellClaim,
    const Mat4<float>& worldTransform)
{
    AFK_World *world                        = afk_core.world;

    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;

    bool success = false;

    /* Check whether we have rendered vapour here.  If we don't,
     * we need to make that.
     */
    if (!shapeCellClaim.getShared().hasVapour(vapourJigsaws))
    {
        /* I need to generate stuff for this cell -- which means I need
         * to upgrade my claim.
         */
        if (shapeCellClaim.upgrade())
        {
            /* Have we already enqueued a different part of this vapour
             * cell for compute?
             */
            unsigned int cubeOffset, cubeCount;
            if (vapourCellClaim.getShared().alreadyEnqueued(cubeOffset, cubeCount))
            {
                int adjacency = vapourCellClaim.getShared().skeletonFullAdjacency(vc, cell, world->sSizes);
                shapeCellClaim.get().enqueueVapourComputeUnitFromExistingVapour(
                    threadId, adjacency, cubeOffset, cubeCount, cell, world->sSizes, vapourJigsaws, world->vapourComputeFair);
                world->shapeVapoursComputed.fetch_add(1);

#if AFK_SHAPE_ENUM_DEBUG
                AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << ": enqueueing existing vapour with " << cubeCount << " cubes from " << cubeOffset << ", from " << vc)
#endif
                    /* As below, now that I no longer compute edges, this is a success condition! */
                success = true;
            }
            else
            {
                /* I need to upgrade my vapour cell claim first */
                if (vapourCellClaim.upgrade())
                {
                    AFK_VapourCell& vapourCell = vapourCellClaim.get();

                    AFK_3DList list;
                    if (vapourCell.build3DList(threadId, vc, list, world->sSizes, vapourCellCache))
                    {
                        int adjacency = vapourCell.skeletonFullAdjacency(vc, cell, world->sSizes);
                        shapeCellClaim.get().enqueueVapourComputeUnitWithNewVapour(
                            threadId, adjacency, list, cell, world->sSizes, vapourJigsaws, world->vapourComputeFair, cubeOffset, cubeCount);
                        vapourCell.enqueued(cubeOffset, cubeCount);
                        world->shapeVapoursComputed.fetch_add(1);
#if AFK_SHAPE_ENUM_DEBUG
                        AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << ": generated new vapour with " << list.cubeCount() << " cubes at " << vc)
#endif

                        /* Now that I no longer compute edges, for now, reaching this
                         * stage indicates success!
                         */
                        success = true;
                    }
                }
            }
        }
    }

    /* TODO: Add in enqueueing of the new 3dnet2 display stuff and
     * all those good things, and remove the success conditions
     * above
     */

    return success;
}

AFK_Shape::AFK_Shape(
    const AFK_ConfigSettings& settings,
    AFK_ThreadAllocation& threadAlloc,
    size_t shapeCacheSize)
{
    /* This is naughty, but I really want an auto-create
     * here.
     */
    size_t shapeCellCacheEntries = shapeCacheSize /
        (2 * settings.shape_skeletonMaxSize * 6 * SQUARE(afk_shapePointSubdivisionFactor));
    shapeCellCache = new AFK_SHAPE_CELL_CACHE(
        4,
        AFK_HashKeyedCell(),
        shapeCellCacheEntries / 2,
        threadAlloc.getNewId());

    size_t vapourCellCacheEntries = shapeCacheSize /
        (2 * settings.shape_skeletonMaxSize * CUBE(afk_shapePointSubdivisionFactor));
    vapourCellCache = new AFK_VAPOUR_CELL_CACHE(
        4,
        AFK_HashKeyedCell(),
        vapourCellCacheEntries / 2,
        threadAlloc.getNewId());
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

