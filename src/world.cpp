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

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "landscape_tile.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng.hpp"
#include "world.hpp"


#define PRINT_CHECKPOINTS 1
#define PRINT_CACHE_STATS 0
#define PRINT_JIGSAW_STATS 0

#define PROTAGONIST_CELL_DEBUG 0


/* The AFK_WorldCellGenParam flags. */
#define AFK_WCG_FLAG_ENTIRELY_VISIBLE   2 /* Cell is already known to be entirely within the viewing frustum */
#define AFK_WCG_FLAG_TERRAIN_RENDER     4 /* Render the terrain regardless of visibility or LoD */
#define AFK_WCG_FLAG_RESUME             8 /* This is a resume after dependent cells were computed */

/* The cell generating worker. */

bool afk_generateWorldCells(
    unsigned int threadId,
    const union AFK_WorldWorkParam& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_Cell cell                 = param.world.cell;
    AFK_World *world                    = param.world.world;

    bool renderTerrain                  = ((param.world.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.world.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    AFK_WorldCell& worldCell = (*(world->worldCache))[cell];

    /* I want an exclusive claim on world cells to stop me from
     * repeating the recursive search process.
     * The render flag means "definitely render this cell's terrain
     * regardless of visibility" (for landscape composition purposes), and the
     * resume flag means "you needed to wait for dependencies, keep
     * trying"; in those cases I don't want an exclusive claim, and
     * I won't do a recursive search.
     */
    if (worldCell.claimYieldLoop(threadId,
        (renderTerrain || resume) ? AFK_CLT_NONEXCLUSIVE : AFK_CLT_EXCLUSIVE,
        afk_core.computingFrame) == AFK_CL_CLAIMED)
    {
        /* This releases the world cell itself when it's done
         * (which isn't at the end of its processing).
         */
        retval = world->generateClaimedWorldCell(
            worldCell, threadId, param.world, queue);
    }

    /* If this cell had a dependency ... */
    if (param.world.dependency)
    {
        if (param.world.dependency->check(queue))
        {
            world->dependenciesFollowed.fetch_add(1);
               delete param.world.dependency;
        }
    }

    return retval;
}


/* AFK_World implementation */

bool AFK_World::checkClaimedLandscapeTile(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    bool display)
{
    /* A LandscapeTile has several stages of creation.
     * First, make sure it's got a terrain descriptor, which
     * will tell us what terrain features go here.
     */
    landscapeTile.makeTerrainDescriptor(
        lSizes,
        tile,
        minCellSize);

    /* Find out whether I'm going to need to be giving this
     * tile some artwork
     */
    bool needsArtwork = false;
    if (display)
    {
        enum AFK_LandscapeTileArtworkState artworkState = landscapeTile.artworkState();
        switch (artworkState)
        {
        case AFK_LANDSCAPE_TILE_NO_PIECE_ASSIGNED:
            needsArtwork = true;
            break;

        case AFK_LANDSCAPE_TILE_PIECE_SWEPT:
            /* I'd like some stats on how often this happens */
            tilesRecomputedAfterSweep.fetch_add(1);
            needsArtwork = true;
            break;

        case AFK_LANDSCAPE_TILE_HAS_ARTWORK:
            needsArtwork = false;
            break;

        default:
            std::ostringstream ss;
            ss << "Unrecognised landscape tile artwork state: " << artworkState;
            throw AFK_Exception(ss.str());
        }
    }
    return needsArtwork;
}


/* Don't enable these unless you want mega spam */
#define DEBUG_JIGSAW_ASSOCIATION 0
#define DEBUG_JIGSAW_ASSOCIATION_GL 0

#define DEBUG_TERRAIN_COMPUTE_QUEUE 0

void AFK_World::generateLandscapeArtwork(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    unsigned int threadId)
{
    /* Create the terrain list, which is composed out of
     * the terrain descriptor for this landscape tile, and
     * those of all the parent tiles (so that child tiles
     * just add detail to an existing terrain).
     */
    AFK_TerrainList terrainList;
    landscapeTile.buildTerrainList(
        threadId,
        terrainList,
        tile,
        subdivisionFactor,
        maxDistance,
        landscapeCache);

    /* Assign a jigsaw piece for this tile, so that the
     * compute phase has somewhere to paint.
     */
    AFK_JigsawPiece jigsawPiece = landscapeTile.getJigsawPiece(
        threadId,
        /* Put bigger tiles only in the first jigsaw, where they won't
         * get swept out as often (they should be longer lived.)
         */
        tile.coord.v[2] >= 256 ? 0 : 1,
        landscapeJigsaws);
#if DEBUG_JIGSAW_ASSOCIATION
    AFK_DEBUG_PRINTL("Compute: " << tile << " -> " << jigsawPiece)
#endif

    /* Now, I need to enqueue the terrain list and the
     * jigsaw piece into one of the rides at the compute fair...
     */
    boost::shared_ptr<AFK_TerrainComputeQueue> computeQueue = landscapeComputeFair.getUpdateQueue(jigsawPiece.puzzle);
    Vec2<int> piece2D = afk_vec2<int>(jigsawPiece.u, jigsawPiece.v);
#if DEBUG_TERRAIN_COMPUTE_QUEUE
    AFK_TerrainComputeUnit unit = computeQueue->extend(
        terrainList, piece2D, &landscapeTile, lSizes);
    AFK_DEBUG_PRINTL("Pushed to queue for " << tile << ": " << unit << ": " << std::endl)
    //AFK_DEBUG_PRINTL(computeQueue->debugTerrain(unit, lSizes))
#else
    computeQueue->extend(terrainList, piece2D, &landscapeTile, lSizes);
#endif

    tilesComputed.fetch_add(1);
}

void AFK_World::generateStartingEntity(
    unsigned int shapeKey,
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    AFK_RNG& rng)
{
    /* I don't need to initialise it here.  We'll come to that when
     * we try to draw it
     */
    worldCell.addStartingEntity(shapeKey, sSizes, rng);
}

bool AFK_World::generateClaimedWorldCell(
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    const struct AFK_WorldWorkParam::World& param,
    AFK_WorldWorkQueue& queue)
{
    const AFK_Cell& cell                = param.cell;
    const Vec3<float>& viewerLocation   = param.viewerLocation;
    const AFK_Camera *camera            = param.camera;

    bool entirelyVisible                = ((param.flags & AFK_WCG_FLAG_ENTIRELY_VISIBLE) != 0);
    bool renderTerrain                  = ((param.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    worldCell.bind(cell, minCellSize);

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;

    if (!entirelyVisible) worldCell.testVisibility(*camera, someVisible, allVisible);
    if (!someVisible && !renderTerrain)
    {
        cellsInvisible.fetch_add(1);

        /* Nothing else to do with it now either. */
        worldCell.release(threadId, AFK_CL_CLAIMED);
    }
    else /* if (cell.coord.v[1] == 0) */
    {
        /* We display geometry at a cell if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell.
         * Note that the smallest possible cell is 2.  I can't go to 1,
         * because it will preclude landscape half-tiles (which would
         * be at +/- 0.5).
         */
        bool display = (cell.coord.v[3] == 2 ||
            worldCell.testDetailPitch(getLandscapeDetailPitch(), *camera, viewerLocation));

        /* Find the tile where any landscape at this cell would be
         * homed
         */
        AFK_Tile tile = afk_tile(cell);

        /* We always at least touch the landscape.  Higher detailed
         * landscape tiles are dependent on lower detailed ones for their
         * terrain description.
         */
        AFK_LandscapeTile& landscapeTile = (*landscapeCache)[tile];
        AFK_ClaimStatus landscapeClaimStatus = landscapeTile.claimYieldLoop(
            threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);

        bool generateArtwork = false;
        if (!landscapeTile.hasTerrainDescriptor() ||
            ((renderTerrain || display) && landscapeTile.artworkState() != AFK_LANDSCAPE_TILE_HAS_ARTWORK))
        {
            /* In order to generate this tile we need to upgrade
             * our claim if we can.
             */
            if (landscapeClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                landscapeClaimStatus = landscapeTile.upgrade(threadId, landscapeClaimStatus);

            if (landscapeClaimStatus == AFK_CL_CLAIMED)
            {
                generateArtwork = checkClaimedLandscapeTile(
                    tile, landscapeTile, display);
            }
            else
            {
                /* Uh oh.  Having an un-generated tile here is not
                 * an option.  Queue a resume of this cell.
                 * Some thread or other will have got the upgradable
                 * claim and will fix it so that when we come back
                 * here, the tile will be generated.
                 */
                AFK_WorldWorkQueue::WorkItem resumeItem;
                resumeItem.func = afk_generateWorldCells;
                resumeItem.param.world = param;
                resumeItem.param.world.flags |= AFK_WCG_FLAG_RESUME;

                if (param.dependency) param.dependency->retain();
                queue.push(resumeItem);
                tilesResumed.fetch_add(1);
            }
        }

        /* If I was picked to generate the geometry, do that. */
        if (generateArtwork)
            generateLandscapeArtwork(tile, landscapeTile, threadId);

        /* At any rate, I should relinquish my claim on the tile
         * now.
         */
        landscapeTile.release(threadId, landscapeClaimStatus);

        if (display)
        {
            if (landscapeTile.artworkState() == AFK_LANDSCAPE_TILE_HAS_ARTWORK)
            {
                /* Get it to make us a unit to
                 * feed into the display queue.
                 * The reason I go through the landscape tile here is so
                 * that it has the opportunity to reject the tile for display
                 * because the cell is entirely outside the y bounds.
                 */
                landscapeClaimStatus = landscapeTile.claimYieldLoop(
                    threadId, AFK_CLT_NONEXCLUSIVE_SHARED, afk_core.computingFrame);
                AFK_JigsawPiece jigsawPiece;
                AFK_LandscapeDisplayUnit unit;
                bool displayThisTile = landscapeTile.makeDisplayUnit(cell, minCellSize, jigsawPiece, unit);
                landscapeTile.release(threadId, landscapeClaimStatus);

                if (displayThisTile)
                {
#if DEBUG_JIGSAW_ASSOCIATION
                    AFK_DEBUG_PRINTL("Display: " << tile << " -> " << jigsawPiece << " -> " << unit)
#endif

                    boost::shared_ptr<AFK_LandscapeDisplayQueue> ldq =
                        landscapeDisplayFair.getUpdateQueue(jigsawPiece.puzzle);

                    ldq->add(unit, &landscapeTile);
                    tilesQueued.fetch_add(1);
                }
            }
        }

        /* Now that I've done all that, I can get to the
         * business of handling the entities within the cell.
         * Firstly, so long as it's above the landscape, give it
         * some starting entities if it hasn't got them
         * already.
         */

        /* TODO: This RNG will make the same set of entities every
         * time the cell is re-displayed.  This is fine for probably
         * static, landscape decoration type objects.  But I'm also
         * going to want to have transient objects popping into
         * existence, and those should be done with a different,
         * long-period RNG that I retain (in thread local storage),
         * perhaps a Mersenne Twister.
         */
        AFK_Boost_Taus88_RNG staticRng;
        staticRng.seed(cell.rngSeed());

        if (worldCell.getRealCoord().v[1] >= landscapeTile.getYBoundUpper())
        {
            unsigned int startingEntityCount = worldCell.getStartingEntitiesWanted(
                staticRng, maxEntitiesPerCell, entitySparseness);

            for (unsigned int e = 0; e < startingEntityCount; ++e)
            {
                unsigned int shapeKey = worldCell.getStartingEntityShapeKey(staticRng);
                generateStartingEntity(shapeKey, worldCell, threadId, staticRng);
            }
        }

        auto eIt = worldCell.entitiesBegin();
        while (eIt != worldCell.entitiesEnd())
        {
            AFK_Entity *e = *eIt;
            auto nextEIt = eIt;
            bool updatedEIt = false;
            if (e->claimYieldLoop(threadId, AFK_CLT_EXCLUSIVE, afk_core.computingFrame) == AFK_CL_CLAIMED)
            {
                /* Make sure everything I need in that shape
                 * has been computed ...
                 */
                AFK_WorldWorkQueue::WorkItem shapeCellItem;
                shapeCellItem.func                          = afk_generateEntity;
                shapeCellItem.param.shape.cell              = afk_keyedCell(afk_vec4<int64_t>(
                                                                0, 0, 0, SHAPE_CELL_MAX_DISTANCE), e->getShapeKey());
                shapeCellItem.param.shape.entity            = e;
                shapeCellItem.param.shape.world             = this;               
                shapeCellItem.param.shape.viewerLocation    = viewerLocation;
                shapeCellItem.param.shape.camera            = camera;
                shapeCellItem.param.shape.flags             = 0;
                shapeCellItem.param.shape.dependency        = nullptr;
                queue.push(shapeCellItem);

                entitiesQueued.fetch_add(1);

                e->release(threadId, AFK_CL_CLAIMED);
            }

            /* Above, we might have updated nextEIt to point to the
             * next item to look at (e.g. if we erased an item).
             * But if we haven't, update the iterator now.
             */
            if (updatedEIt) eIt = nextEIt;
            else ++eIt;
        }

        /* Pop any new entities out from the move queue into the
         * proper list.
         */
        worldCell.popMoveQueue();

        /* We don't need this any more */
        worldCell.release(threadId, AFK_CL_CLAIMED);

        /* If the terrain here was at too coarse a resolution to
         * be displayable, recurse through the subcells
         */
        if (!display && !renderTerrain && someVisible && !resume)
        {
            size_t subcellsSize = CUBE(subdivisionFactor);
            AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow.  Maybe make it an iterator */
            unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize, subdivisionFactor);
            assert(subcellsCount == subcellsSize);

            for (unsigned int i = 0; i < subcellsCount; ++i)
            {
                AFK_WorldWorkQueue::WorkItem subcellItem;
                subcellItem.func                     = afk_generateWorldCells;
                subcellItem.param.world.cell         = subcells[i];
                subcellItem.param.world.world        = param.world;
                subcellItem.param.world.viewerLocation = viewerLocation;
                subcellItem.param.world.camera       = camera;
                subcellItem.param.world.flags        = (allVisible ? AFK_WCG_FLAG_ENTIRELY_VISIBLE : 0);
                subcellItem.param.world.dependency   = nullptr;
                queue.push(subcellItem);
            }

            delete[] subcells;
        }
    }
#if 0
    else
    {
        worldCell.release(threadId, AFK_CL_CLAIMED);
    }
#endif

    return retval;
}

AFK_World::AFK_World(
    const AFK_Config *config,
    const AFK_Computer *computer,
    float _maxDistance,
    unsigned int worldCacheSize,
    unsigned int tileCacheSize,
    unsigned int shapeCacheSize,
    cl_context ctxt,
    AFK_RNG *setupRng):
        startingDetailPitch         (config->startingDetailPitch),
        maxDetailPitch              (config->maxDetailPitch),
        detailPitch                 (config->startingDetailPitch), /* This is a starting point */
        averageDetailPitch          (config->framesPerCalibration, config->startingDetailPitch),
        shape                       (config, shapeCacheSize),
        maxDistance                 (_maxDistance),
        subdivisionFactor           (config->subdivisionFactor),
        minCellSize                 (config->minCellSize),
        lSizes                      (config),
        sSizes                      (config),
        maxEntitiesPerCell          (config->maxEntitiesPerCell),
        entitySparseness            (config->entitySparseness)

{
    /* Set up the caches and generator gang. */

    unsigned int tileCacheEntries = tileCacheSize / lSizes.tSize;
    Vec3<int> tilePieceSize = afk_vec3<int>((int)lSizes.tDim, (int)lSizes.tDim, 1);

    landscapeJigsaws = new AFK_JigsawCollection(
        ctxt,
        tilePieceSize,
        (int)tileCacheEntries,
        2, /* I want at least two, so I can put big tiles only into the first one */
        AFK_JIGSAW_2D,
        {
            AFK_JIGSAW_FLOAT32,         /* Y displacement */
            AFK_JIGSAW_4FLOAT8_UNORM,   /* Colour */
            config->clGlSharing ? AFK_JIGSAW_4FLOAT32 : AFK_JIGSAW_4FLOAT8_SNORM
                                        /* Normal; packed 8-bit signed doesn't seem to
                                         * play nice with cl_gl buffer sharing */
        },
        computer->getFirstDeviceProps(),
        config->clGlSharing ? AFK_JIGSAW_BU_CL_GL_SHARED : AFK_JIGSAW_BU_CL_GL_COPIED,
        config->concurrency,
        false);

    tileCacheEntries = landscapeJigsaws->getPieceCount();
    unsigned int tileCacheBitness = afk_suggestCacheBitness(tileCacheEntries);

    landscapeCache = new AFK_LANDSCAPE_CACHE(
        tileCacheBitness, 8, AFK_HashTile(), tileCacheEntries / 2, 0xffffffffu);

    /* TODO Right now I don't have a sensible value for the
     * world cache size, because I'm not doing any exciting
     * geometry work with it.  When I am, I'll get that.
     * But for now, just make a guess.
     */
    unsigned int worldCacheEntrySize = SQUARE(lSizes.pointSubdivisionFactor);
    unsigned int worldCacheEntries = worldCacheSize / worldCacheEntrySize;
    unsigned int worldCacheBitness = afk_suggestCacheBitness(worldCacheEntries);

    worldCache = new AFK_WORLD_CACHE(
        worldCacheBitness, 8, AFK_HashCell(), worldCacheEntries, 0xfffffffeu);

    unsigned int shapeCacheEntries = shapeCacheSize / (32 * SQUARE(sSizes.eDim) * 6 + 16 * CUBE(sSizes.tDim));

    Vec3<int> vapourPieceSize = afk_vec3<int>(sSizes.tDim, sSizes.tDim, sSizes.tDim);

    vapourJigsaws = new AFK_JigsawCollection(
        ctxt,
        vapourPieceSize,
        (int)shapeCacheEntries,
        1,
        AFK_JIGSAW_3D,
        {
            AFK_JIGSAW_4FLOAT32,                    /* Feature: colour and density.
                                                     * TODO: Try to cram these: colour only needs 8
                                                     * bits per channel.
                                                     */
            config->clGlSharing ? AFK_JIGSAW_4FLOAT32 : AFK_JIGSAW_4FLOAT8_SNORM
                                                    /* Normal, with TODO as above. */
        },
        computer->getFirstDeviceProps(),
        /* config->clGlSharing ? AFK_JIGSAW_BU_CL_GL_SHARED : AFK_JIGSAW_BU_CL_GL_COPIED */ AFK_JIGSAW_BU_CL_ONLY /* <-- TODO put that back */,
        config->concurrency,
        computer->useFake3DImages(config));

    /* Each edge piece will have 3 faces horizontally by 2 vertically to
     * cram the 6 faces together in a better manner than stringing them
     * in a line.
     */
    Vec3<int> edgePieceSize = afk_vec3<int>(sSizes.eDim * 3, sSizes.eDim * 2, 1);

    edgeJigsaws = new AFK_JigsawCollection(
        ctxt,
        edgePieceSize,
        (int)shapeCacheEntries,
        1,
        AFK_JIGSAW_2D,
        {                               /* TODO: When I've got glsl reading out of the vapour
                                         * correctly, prune this list!
                                         */
            AFK_JIGSAW_4FLOAT32,        /* Displacement */
            AFK_JIGSAW_4FLOAT8_UNORM,   /* Colour */
            config->clGlSharing ? AFK_JIGSAW_4FLOAT32 : AFK_JIGSAW_4FLOAT8_SNORM,
                                        /* Normal */
            AFK_JIGSAW_2UINT32          /* Overlap */
        },
        computer->getFirstDeviceProps(),
        config->clGlSharing ? AFK_JIGSAW_BU_CL_GL_SHARED : AFK_JIGSAW_BU_CL_GL_COPIED,
        config->concurrency,
        false);

    genGang = new AFK_AsyncGang<union AFK_WorldWorkParam, bool>(
        100, config->concurrency);

    /* Set up the landscape shader. */
    landscape_shaderProgram = new AFK_ShaderProgram();
    *landscape_shaderProgram << "landscape_fragment" << "landscape_geometry" << "landscape_vertex";
    landscape_shaderProgram->Link();

    landscape_shaderLight = new AFK_ShaderLight(landscape_shaderProgram->program);
    landscape_clipTransformLocation = glGetUniformLocation(landscape_shaderProgram->program, "ClipTransform");
    landscape_skyColourLocation = glGetUniformLocation(landscape_shaderProgram->program, "SkyColour");
    landscape_farClipDistanceLocation = glGetUniformLocation(landscape_shaderProgram->program, "FarClipDistance");

    entity_shaderProgram = new AFK_ShaderProgram();
    *entity_shaderProgram << "shape_fragment" << "shape_geometry" << "shape_vertex";
    entity_shaderProgram->Link();

    entity_shaderLight = new AFK_ShaderLight(entity_shaderProgram->program);
    entity_projectionTransformLocation = glGetUniformLocation(entity_shaderProgram->program, "ProjectionTransform");
    entity_skyColourLocation = glGetUniformLocation(entity_shaderProgram->program, "SkyColour");
    entity_farClipDistanceLocation = glGetUniformLocation(entity_shaderProgram->program, "FarClipDistance");

    glGenVertexArrays(1, &landscapeTileArray);
    glBindVertexArray(landscapeTileArray);
    landscapeTerrainBase = new AFK_TerrainBaseTile(lSizes);
    landscapeTerrainBase->initGL();

    /* Done. */
    glBindVertexArray(0);
    landscapeTerrainBase->teardownGL();

    glGenVertexArrays(1, &edgeShapeBaseArray);
    glBindVertexArray(edgeShapeBaseArray);
    edgeShapeBase = new AFK_3DEdgeShapeBase(sSizes);
    edgeShapeBase->initGL();

    glBindVertexArray(0);
    edgeShapeBase->teardownGL();

    /* Make the base colour for the landscape here */
    landscape_baseColour = afk_vec3<float>(
        setupRng->frand(), setupRng->frand(), setupRng->frand());

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    tilesQueued.store(0);
    tilesResumed.store(0);
    tilesComputed.store(0);
    tilesRecomputedAfterSweep.store(0);
    entitiesQueued.store(0);
    entitiesMoved.store(0);
    shapeCellsInvisible.store(0);
    shapeCellsResumed.store(0);
    shapeVapoursComputed.store(0);
    shapeEdgesComputed.store(0);
    threadEscapes.store(0);
}

AFK_World::~AFK_World()
{
    /* Delete the generator gang first, so that I know that
     * any worker threads have stopped before I start deleting
     * the things they're using.
     */
    delete genGang;

    if (landscapeJigsaws) delete landscapeJigsaws;
    delete landscapeCache;
    delete worldCache;

    if (edgeJigsaws) delete edgeJigsaws;
    if (vapourJigsaws) delete vapourJigsaws;

    delete landscape_shaderProgram;
    delete landscape_shaderLight;

    delete entity_shaderProgram;
    delete entity_shaderLight;

    delete edgeShapeBase;
    glDeleteVertexArrays(1, &edgeShapeBaseArray);

    delete landscapeTerrainBase;
    glDeleteVertexArrays(1, &landscapeTileArray);
}

void AFK_World::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<int64_t>& modifier,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell = afk_cell(afk_vec4<int64_t>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[1] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[2] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));

    AFK_WorldWorkQueue::WorkItem cellItem;
    cellItem.func                        = afk_generateWorldCells;
    cellItem.param.world.cell            = modifiedCell;
    cellItem.param.world.world           = this;
    cellItem.param.world.viewerLocation  = viewerLocation;
    cellItem.param.world.camera          = &camera;
    cellItem.param.world.flags           = 0;
    cellItem.param.world.dependency      = nullptr;
    (*genGang) << cellItem;
}

void AFK_World::flipRenderQueues(cl_context ctxt, const AFK_Frame& newFrame)
{
    /* Verify that the concurrency control business has done
     * its job correctly.
     */
    if (!genGang->assertNoQueuedWork())
        threadEscapes.fetch_add(1);

    landscapeComputeFair.flipQueues();
    landscapeDisplayFair.flipQueues();
    landscapeJigsaws->flipCuboids(ctxt, newFrame);

    vapourComputeFair.flipQueues();
    edgeComputeFair.flipQueues();
    entityDisplayFair.flipQueues();
    vapourJigsaws->flipCuboids(ctxt, newFrame);
    edgeJigsaws->flipCuboids(ctxt, newFrame);
}

void AFK_World::alterDetail(float adjustment)
{
    float adj = std::max(std::min(adjustment, 1.2f), 0.85f);

    if (adj > 1.0f || !worldCache->wayOutsideTargetSize())
        detailPitch = std::min(detailPitch * adjustment, maxDetailPitch);

    averageDetailPitch.push(detailPitch);
}

float AFK_World::getLandscapeDetailPitch(void) const
{
    return averageDetailPitch.get();
}

float AFK_World::getEntityDetailPitch(void) const
{
    return averageDetailPitch.get() *
        (float)sSizes.pointSubdivisionFactor / (float)lSizes.pointSubdivisionFactor;
}

boost::unique_future<bool> AFK_World::updateWorld(void)
{
    /* Maintenance. */
    landscapeCache->doEvictionIfNecessary();
    worldCache->doEvictionIfNecessary();
    shape.updateWorld();

    /* First, transform the protagonist location and its facing
     * into integer cell-space.
     */
    Mat4<float> protagonistTransformation = afk_core.protagonist->object.getTransformation();
    Vec4<float> hgProtagonistLocation = protagonistTransformation *
        afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
    Vec3<float> protagonistLocation = afk_vec3<float>(
        hgProtagonistLocation.v[0] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[1] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[2] / hgProtagonistLocation.v[3]);
    Vec4<int64_t> csProtagonistLocation = afk_vec4<int64_t>(
        (int64_t)(protagonistLocation.v[0] / minCellSize),
        (int64_t)(protagonistLocation.v[1] / minCellSize),
        (int64_t)(protagonistLocation.v[2] / minCellSize),
        1);

    AFK_Cell protagonistCell = afk_cell(csProtagonistLocation);

#ifdef PROTAGONIST_CELL_DEBUG
    {
        std::ostringstream ss;
        ss << "Protagonist in cell: " << protagonistCell;
        afk_core.occasionallyPrint(ss.str());
    }
#endif

    /* Wind up the cell tree and find its largest parent,
     * then go one step smaller -- because a too-big cell
     * just won't be seen by the projection and will
     * therefore be culled
     */
    AFK_Cell cell, parentCell;
    for (cell = parentCell = protagonistCell;
        (float)cell.coord.v[3] < maxDistance;
        cell = parentCell, parentCell = cell.parent(subdivisionFactor));

    /* Draw that cell and the other cells around it.
     */
    for (int64_t i = -1; i <= 1; ++i)
        for (int64_t j = -1; j <= 1; ++j)
            for (int64_t k = -1; k <= 1; ++k)
                enqueueSubcells(cell, afk_vec3<int64_t>(i, j, k), protagonistLocation, *(afk_core.camera));

    return genGang->start();
}

void AFK_World::doComputeTasks(void)
{
    /* The fair organises the terrain lists and jigsaw pieces by
     * puzzle, so that I can easily batch up work that applies to the
     * same jigsaw:
     */
    std::vector<boost::shared_ptr<AFK_TerrainComputeQueue> > terrainComputeQueues;
    landscapeComputeFair.getDrawQueues(terrainComputeQueues);

    /* The fair's queues are in the same order as the puzzles in
     * the jigsaw collection.
     */
    for (unsigned int puzzle = 0; puzzle < terrainComputeQueues.size(); ++puzzle)
    {
        terrainComputeQueues[puzzle]->computeStart(afk_core.computer, landscapeJigsaws->getPuzzle(puzzle), lSizes, landscape_baseColour);
    }

    std::vector<boost::shared_ptr<AFK_3DVapourComputeQueue> > vapourComputeQueues;
    vapourComputeFair.getDrawQueues(vapourComputeQueues);
    assert(vapourComputeQueues.size() <= 1);
    if (vapourComputeQueues.size() == 1)
        vapourComputeQueues[0]->computeStart(afk_core.computer, vapourJigsaws, sSizes);

    std::vector<boost::shared_ptr<AFK_3DEdgeComputeQueue> > edgeComputeQueues;
    edgeComputeFair.getDrawQueues(edgeComputeQueues);

    for (unsigned int puzzle = 0; puzzle < edgeComputeQueues.size(); ++puzzle)
    {
        edgeComputeQueues[puzzle]->computeStart(
            afk_core.computer,
            vapourJigsaws,
            edgeJigsaws->getPuzzle(puzzle),
            sSizes);
    }

    /* If I finalise stuff now, the y-reduce information will
     * be in the landscape tiles in time for the display
     * to edit out any cells I now know to be empty of terrain.
     */
    for (unsigned int puzzle = 0; puzzle < terrainComputeQueues.size(); ++puzzle)
    {
        terrainComputeQueues[puzzle]->computeFinish();
    }

    if (vapourComputeQueues.size() == 1)
        vapourComputeQueues[0]->computeFinish();

    for (unsigned int puzzle = 0; puzzle < edgeComputeQueues.size(); ++puzzle)
    {
        edgeComputeQueues[puzzle]->computeFinish();
    }
}

void AFK_World::display(const Mat4<float>& projection, const AFK_Light &globalLight)
{
    /* Render the landscape */
    glUseProgram(landscape_shaderProgram->program);
    landscape_shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(landscape_clipTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform3fv(landscape_skyColourLocation, 1, &afk_core.skyColour.v[0]);
    glUniform1f(landscape_farClipDistanceLocation, afk_core.config->zFar);
    AFK_GLCHK("landscape uniforms")

    glBindVertexArray(landscapeTileArray);
    AFK_GLCHK("landscape bindVertexArray")


    /* Now that I've set that up, make the texture that describes
     * where the tiles are in space ...
     */
    std::vector<boost::shared_ptr<AFK_LandscapeDisplayQueue> > landscapeDrawQueues;
    landscapeDisplayFair.getDrawQueues(landscapeDrawQueues);

    /* Those queues are in puzzle order. */
    for (unsigned int puzzle = 0; puzzle < landscapeDrawQueues.size(); ++puzzle)
    {
        landscapeDrawQueues[puzzle]->draw(landscape_shaderProgram, landscapeJigsaws->getPuzzle(puzzle), landscapeTerrainBase, lSizes);
    }

    glBindVertexArray(0);

    /* Render the shapes */
    glUseProgram(entity_shaderProgram->program);
    entity_shaderLight->setupLight(globalLight);
    glUniformMatrix4fv(entity_projectionTransformLocation, 1, GL_TRUE, &projection.m[0][0]);
    glUniform3fv(entity_skyColourLocation, 1, &afk_core.skyColour.v[0]);
    glUniform1f(entity_farClipDistanceLocation, afk_core.config->zFar);
    AFK_GLCHK("shape uniforms")

    glBindVertexArray(edgeShapeBaseArray);
    AFK_GLCHK("edge shape bindVertexArray")

    std::vector<boost::shared_ptr<AFK_EntityDisplayQueue> > entityDrawQueues;
    entityDisplayFair.getDrawQueues(entityDrawQueues);

    for (unsigned int puzzle = 0; puzzle < entityDrawQueues.size(); ++puzzle)
    {
        entityDrawQueues[puzzle]->draw(entity_shaderProgram, edgeJigsaws->getPuzzle(puzzle), edgeShapeBase, sSizes);
    }

    glBindVertexArray(0);
}

/* Worker for the below. */
#if PRINT_CHECKPOINTS
static float toRatePerSecond(uint64_t quantity, boost::posix_time::time_duration& interval)
{
    return (float)quantity * 1000.0f / (float)interval.total_milliseconds();
}

#define PRINT_RATE_AND_RESET(s, v) std::cout << s << toRatePerSecond((v).exchange(0), timeSinceLastCheckpoint) << "/second" << std::endl;
#endif

void AFK_World::checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint)
{
#if PRINT_CHECKPOINTS
    std::cout << "Detail pitch:                 " << detailPitch << std::endl;
    PRINT_RATE_AND_RESET("Cells found invisible:        ", cellsInvisible)
    PRINT_RATE_AND_RESET("Tiles queued:                 ", tilesQueued)
    PRINT_RATE_AND_RESET("Tiles resumed:                ", tilesResumed)
    PRINT_RATE_AND_RESET("Tiles computed:               ", tilesComputed)
    PRINT_RATE_AND_RESET("Tiles recomputed after sweep: ", tilesRecomputedAfterSweep)
    PRINT_RATE_AND_RESET("Entities queued:              ", entitiesQueued)
    PRINT_RATE_AND_RESET("Entities moved:               ", entitiesMoved)
    PRINT_RATE_AND_RESET("Shape cells found invisible:  ", shapeCellsInvisible)
    PRINT_RATE_AND_RESET("Shape cells resumed:          ", shapeCellsResumed)
    PRINT_RATE_AND_RESET("Shape vapours computed:       ", shapeVapoursComputed)
    PRINT_RATE_AND_RESET("Shape edges computed:         ", shapeEdgesComputed)
    PRINT_RATE_AND_RESET("Separate vapours computed:    ", separateVapoursComputed)
    PRINT_RATE_AND_RESET("Dependencies followed:        ", dependenciesFollowed)
    std::cout <<         "Cumulative thread escapes:    " << threadEscapes.load() << std::endl;
#endif
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
#if PRINT_CACHE_STATS
    worldCache->printStats(ss, "World cache");
    landscapeCache->printStats(ss, "Landscape cache");
    shape.printCacheStats(ss, prefix);
#endif
}

void AFK_World::printJigsawStats(std::ostream& ss, const std::string& prefix)
{
#if PRINT_JIGSAW_STATS
    landscapeJigsaws->printStats(ss, "Landscape");
    vapourJigsaws->printStats(ss, "Vapour");
    edgeJigsaws->printStats(ss, "Edge");
#endif
}

