/* AFK
 * Copyright (C) 2013, Alex Holloway.
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

#include <boost/functional/hash.hpp>
#include <boost/shared_ptr.hpp>

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
    AFK_WorldWorkQueue& queue)
{
    AFK_KeyedCell cell                      = param.shape.cell;
    AFK_World *world                        = param.shape.world;

    AFK_Shape& shape                        = world->shape;

    bool resume = false;

    AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
    auto claim = (*(shape.vapourCellCache))[vc].claimable.claim(threadId, AFK_CL_LOOP | AFK_CL_SHARED);

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
        resume = true;
    }

    /* I don't need this any more */
    claim.release();

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
    Mat4<float> worldTransform              = param.shape.transformation;
    AFK_World *world                        = param.shape.world;
    const Vec3<float>& viewerLocation       = param.shape.viewerLocation;
    const AFK_Camera *camera                = param.shape.camera;

    bool entirelyVisible                    = ((param.shape.flags & AFK_SCG_FLAG_ENTIRELY_VISIBLE) != 0);

    AFK_Shape& shape                        = world->shape;

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
                world->getEntityDetailPitch(), *camera, viewerLocation));

        /* Always build the vapour descriptor, because other cells
         * will need it.
         * Only do the render if we actually intend to display the
         * cell, however.
         */
        AFK_KeyedCell vc = afk_shapeToVapourCell(cell, world->sSizes);
        auto vapourCellClaim = (*(shape.vapourCellCache))[vc].claimable.claim(threadId, AFK_CL_LOOP | AFK_CL_SHARED);
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
                    shape.vapourCellCache->at(upperVC).claimable.claim(threadId, AFK_CL_LOOP | AFK_CL_SHARED);
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
                if (display) 
                {
                    /* I want that shape cell now ... */
                    auto shapeCellClaim = (*(shape.shapeCellCache))[cell].claimable.claim(threadId, AFK_CL_LOOP | AFK_CL_SHARED);
                    if (!shape.generateClaimedShapeCell(
                        threadId, vc, cell, vapourCellClaim, shapeCellClaim, worldTransform, visibleCell, world))
                    {
                        DEBUG_VISIBLE_CELL("needs resume")
                        resume = true;
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
                DEBUG_VISIBLE_CELL("outside skeleton")
#if AFK_SHAPE_ENUM_DEBUG
                AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << " of entity: worldCell=" << param.shape.asedWorldCell << ", entity counter=" << param.shape.asedCounter << " outsideskeleton")
#endif
            }
        }
        else
        {
            DEBUG_VISIBLE_CELL("needs resume at top level")
            resume = true;
        }
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
    const AFK_KeyedCell& vc,
    const AFK_KeyedCell& cell,
    AFK_CLAIM_OF(VapourCell)& vapourCellClaim,
    AFK_CLAIM_OF(ShapeCell)& shapeCellClaim,
    const Mat4<float>& worldTransform,
    const AFK_VisibleCell& visibleCell,
    AFK_World *world)
{
    AFK_JigsawCollection *vapourJigsaws     = world->vapourJigsaws;
    AFK_JigsawCollection *edgeJigsaws       = world->edgeJigsaws;

    const AFK_ShapeCell& shapeCell = shapeCellClaim.getShared();
    bool success = false;

    /* Check whether we have rendered vapour here.  If we don't,
     * we need to make that first.
     */
    if (!shapeCell.hasVapour(vapourJigsaws))
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
                    threadId, adjacency, cubeOffset, cubeCount, visibleCell.getRealCoord(), cell.key, world->sSizes, vapourJigsaws, world->vapourComputeFair);
                world->shapeVapoursComputed.fetch_add(1);

#if AFK_SHAPE_ENUM_DEBUG
                AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << ": enqueueing existing vapour with " << cubeCount << " cubes from " << cubeOffset << ", from " << vc)
#endif
            }
            else
            {
                /* I need to upgrade my vapour cell claim first */
                if (vapourCellClaim.upgrade())
                {
                    AFK_VapourCell& vapourCell = vapourCellClaim.get();

                    AFK_3DList list;
                    vapourCell.build3DList(threadId, vc, list, world->sSizes, vapourCellCache);

                    int adjacency = vapourCell.skeletonFullAdjacency(vc, cell, world->sSizes);
                    shapeCellClaim.get().enqueueVapourComputeUnitWithNewVapour(
                        threadId, adjacency, list, visibleCell.getRealCoord(), cell.key, world->sSizes, vapourJigsaws, world->vapourComputeFair, cubeOffset, cubeCount);
                    vapourCell.enqueued(cubeOffset, cubeCount);
                    world->shapeVapoursComputed.fetch_add(1);
#if AFK_SHAPE_ENUM_DEBUG
                    AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << ": generated new vapour with " << list.cubeCount() << " cubes at " << vc)
#endif
                }
            }
        }
    }

    /* Try to get this shape cell set up and enqueued.
     */
    if (!shapeCell.hasEdges(edgeJigsaws))
    {
        /* I need to generate stuff for this cell -- which means I need
         * to upgrade my claim.
         */
        if (shapeCellClaim.upgrade())
        {
            /* The vapour descriptor must be there already. */
            assert(vapourCellClaim.getShared().hasDescriptor());

            if (shapeCell.hasVapour(vapourJigsaws))
            {
                shapeCellClaim.get().enqueueEdgeComputeUnit(
                    threadId, shapeCellCache, vapourJigsaws, edgeJigsaws, world->edgeComputeFair, world->entityFair2DIndex);
                world->shapeEdgesComputed.fetch_add(1);

#if AFK_SHAPE_ENUM_DEBUG
                AFK_DEBUG_PRINTL("ASED: Shape cell " << cell << ": generated edges from vapour at " << vc)
#endif
            }
        }
    }

    if (shapeCell.hasVapour(vapourJigsaws) &&
        shapeCell.hasEdges(edgeJigsaws))
    {
        shapeCell.enqueueEdgeDisplayUnit(
            worldTransform,
            visibleCell.getHomogeneous(),
            vapourJigsaws,
            edgeJigsaws,
            world->entityDisplayFair,
            world->entityFair2DIndex);
        success = true;
    }

    return success;
}

AFK_Shape::AFK_Shape(
    const AFK_Config *config,
    AFK_ThreadAllocation& threadAlloc,
    unsigned int shapeCacheSize)
{
    /* This is naughty, but I really want an auto-create
     * here.
     */
    unsigned int shapeCellCacheEntries = shapeCacheSize /
        (2 * config->shape_skeletonMaxSize * 6 * SQUARE(afk_shapePointSubdivisionFactor));
    shapeCellCache = new AFK_SHAPE_CELL_CACHE(
        4,
        AFK_HashKeyedCell(),
        shapeCellCacheEntries / 2,
        threadAlloc.getNewId());

    unsigned int vapourCellCacheEntries = shapeCacheSize /
        (2 * config->shape_skeletonMaxSize * CUBE(afk_shapePointSubdivisionFactor));
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

