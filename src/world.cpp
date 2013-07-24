/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng.hpp"
#include "world.hpp"

/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1


/* The AFK_WorldCellGenParam flags. */
#define AFK_WCG_FLAG_ENTIRELY_VISIBLE   2 /* Cell is already known to be entirely within the viewing frustum */
#define AFK_WCG_FLAG_TERRAIN_RENDER     4 /* Render the terrain regardless of visibility or LoD */
#define AFK_WCG_FLAG_RESUME             8 /* This is a resume after dependent cells were computed */

/* The cell generating worker. */

bool afk_generateWorldCells(
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue)
{
    const AFK_Cell& cell                = param.cell;
    AFK_World *world                    = param.world;

    bool renderTerrain                  = ((param.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

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
        (renderTerrain || resume) ? AFK_CLT_NONEXCLUSIVE : AFK_CLT_EXCLUSIVE) == AFK_CL_CLAIMED)
    {
        /* This releases the world cell itself when it's done
         * (which isn't at the end of its processing).
         */
        retval = world->generateClaimedWorldCell(
            worldCell, threadId, param, queue);
    }

    /* If this cell had a dependency ... */
    if (param.dependency)
    {
        if (param.dependency->count.fetch_sub(1) == 1)
        {
            /* That dependency is complete.  Enqueue the final cell
             * and delete it.
             */
            queue.push(*(param.dependency->finalCell));
            delete param.dependency->finalCell;
            delete param.dependency;
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

    /* Tile colour debugging goes here. */
    Vec3<float>* forcedTint = NULL;
    landscapeTile.makeTerrainDescriptor(
        pointSubdivisionFactor,
        subdivisionFactor,
        tile,
        minCellSize,
        forcedTint);

    /* Find out whether I'm going to be computing the geometry */
    bool haveGeometryRights = (display && landscapeTile.claimGeometryRights());
    return haveGeometryRights;
}

void AFK_World::generateLandscapeGeometry(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    unsigned int threadId)
{
    landscapeTile.makeRawTerrain(AFK_LANDSCAPE_TYPE, tile, pointSubdivisionFactor, minCellSize);

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

    /* Using that terrain list, compute the landscape geometry. */
    landscapeTile.computeGeometry(pointSubdivisionFactor, tile, terrainList);
    tilesComputed.fetch_add(1);
}

bool AFK_World::generateClaimedWorldCell(
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue)
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
    else
    {
        /* We display geometry at a cell if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell
         */
        bool display = (cell.coord.v[3] == MIN_CELL_PITCH ||
            worldCell.testDetailPitch(renderDetailPitch, *camera, viewerLocation));

        /* TODO: Non-landscape stuff goes here.  :-) */

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
            threadId, AFK_CLT_NONEXCLUSIVE_SHARED);

        bool generateGeometry = false;
        if (!landscapeTile.hasTerrainDescriptor() ||
            ((renderTerrain || display) && !landscapeTile.hasGeometry()))
        {
            /* In order to generate this tile we need to upgrade
             * our claim if we can.
             */
            if (landscapeClaimStatus == AFK_CL_CLAIMED_UPGRADABLE)
                landscapeClaimStatus = landscapeTile.upgrade(threadId, landscapeClaimStatus);

            if (landscapeClaimStatus == AFK_CL_CLAIMED)
            {
                generateGeometry = checkClaimedLandscapeTile(
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
                struct AFK_WorldCellGenParam resumeParam(param);
                resumeParam.flags |= AFK_WCG_FLAG_RESUME;
                queue.push(resumeParam);
                tilesResumed.fetch_add(1);
            }
        }

        /* If I was picked to generate the geometry, do that. */
        if (generateGeometry)
            generateLandscapeGeometry(tile, landscapeTile, threadId);

        /* At any rate, I should relinquish my claim on the tile
         * now.
         * TODO Why can't I do that earlier?
         */
        landscapeTile.release(threadId, landscapeClaimStatus);

        if (display)
        {
            /* TODO Putting some metrics in here to try to understand what
             * the problem with relinquishing the exclusive lock early is.
             */

            if (landscapeTile.hasGeometry())
            {
                /* Get it to make us a DisplayedLandscapeTile to
                 * feed into the display queue.
                 */
                landscapeClaimStatus = landscapeTile.claimYieldLoop(
                    threadId, AFK_CLT_NONEXCLUSIVE_SHARED);
                AFK_DisplayedLandscapeTile *dptr =
                    landscapeTile.makeDisplayedLandscapeTile(cell, minCellSize);
                landscapeTile.release(threadId, landscapeClaimStatus);

                if (dptr)
                {
                    landscapeRenderQueue.update_push(dptr);
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

        if (!landscapeTile.hasGeometry() ||
            worldCell.getRealCoord().v[1] >= landscapeTile.getYBoundUpper())
        {
            worldCell.doStartingEntities(pointSubdivisionFactor, subdivisionFactor, staticRng);
        }

        AFK_ENTITY_LIST::iterator eIt = worldCell.entitiesBegin();
        while (eIt != worldCell.entitiesEnd())
        {
            AFK_Entity *e = *eIt;
            if (e->claimYieldLoop(threadId, AFK_CLT_EXCLUSIVE) == AFK_CL_CLAIMED)
            {
                AFK_Cell newCell;
                if (e->animate(afk_core.getStartOfFrameTime(), cell, minCellSize, newCell))
                {
                    /* This entity has moved to a different cell.
                     * Dig it up and feed it into its entity list.
                     */
                    
                    /* Yes, I'm going to be creating this cell if
                     * it doesn't exist.  And not claiming it.  So
                     * there's a chance the entity gets cleaned up
                     * by the evictor.  I can handle with that.
                     * TODO: Permanent entities -- should prevent
                     * the evictor from getting rid of cells that
                     * contain them, and should also cause their
                     * cells to always get enumerated regardless of
                     * angle and distance.
                     */
                    eIt = worldCell.eraseEntity(eIt);

                    /* TODO: There is a very tiny possibility of the
                     * following scenario here:
                     * - cell gets created in the cache
                     * - cell gets grabbed by the eviction thread and
                     * destroyed
                     * - moveEntity() gets called on a dead cell and
                     * crashes
                     * To avoid this I need to ensure that cells are
                     * naturally created in an un-evictable state.  I'm
                     * not quite sure how to ensure that.
                     */
                    (*worldCache)[newCell].moveEntity(e);
                    entitiesMoved.fetch_add(1);
                }
                else ++eIt;

                /* TODO: Collisions probably go here.  I'll no doubt
                 * want to do that in OpenCL for performance, and
                 * all sorts.
                 */

                /* Entities are always displayed.  They don't
                 * exist at multiple LoDs.
                 * TODO: Maybe some day I'll want to make very
                 * large entities out of component parts so
                 * that I can zoom in on their LoD just like
                 * I can with the landscape ?
                 */

                /* TODO: Figure out the list of lights that apply.
                 * And some day, the list of shadows that apply,
                 * too.  Rah!
                 */
                AFK_DisplayedEntity *de = e->makeDisplayedEntity();
                if (de)
                {
                    entityRenderQueue.update_push(de);
                    entitiesQueued.fetch_add(1);
                }

                e->release(threadId, AFK_CL_CLAIMED);
            }
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

            if (subcellsCount == subcellsSize)
            {
                for (unsigned int i = 0; i < subcellsCount; ++i)
                {
                    struct AFK_WorldCellGenParam subcellParam;
                    subcellParam.cell               = subcells[i];
                    subcellParam.world              = param.world;
                    subcellParam.viewerLocation     = viewerLocation;
                    subcellParam.camera             = camera;
                    subcellParam.flags              = (allVisible ? AFK_WCG_FLAG_ENTIRELY_VISIBLE : 0);
                    subcellParam.dependency         = NULL;
                    queue.push(subcellParam);
                }
            }
            else
            {
                /* That's clearly a bug :P */
                std::ostringstream ss;
                ss << "Cell " << cell << " subdivided into " << subcellsCount << " not " << subcellsSize;
                throw AFK_Exception(ss.str());
            }
        
            delete[] subcells;
        }
    }

    return retval;
}

AFK_World::AFK_World(
    float _maxDistance,
    unsigned int _subdivisionFactor,
    unsigned int _pointSubdivisionFactor,
    float _minCellSize,
    float _startingDetailPitch):
        startingDetailPitch(_startingDetailPitch),
        detailPitch(_startingDetailPitch), /* This is a starting point */
        renderDetailPitch(_startingDetailPitch),
        landscapeRenderQueue(10000),
        entityRenderQueue(10000),
        maxDistance(_maxDistance),
        subdivisionFactor(_subdivisionFactor),
        pointSubdivisionFactor(_pointSubdivisionFactor),
        minCellSize(_minCellSize)
{
    /* Set up the caches and generator gang. */

    /* TODO: For the cache sizes, try to compute suitable sizes based
     * on system memory (and GPU memory for the dwc cache), and derive
     * bitnesses and contention targets from those values
     */
    worldCache = new AFK_WORLD_CACHE(
        22, 8, AFK_HashCell(), 240000, 0xfffffffeu);

    landscapeCache = new AFK_LANDSCAPE_CACHE(
        18, 8, AFK_HashTile(), 24000, 0xfffffffdu);

    /* Note that loading lots of extra threads here does not seem
     * to make us do significantly better than the default of
     * (machine thread count + 1)
     */
    genGang = new AFK_AsyncGang<struct AFK_WorldCellGenParam, bool>(
            boost::function<bool (unsigned int, const struct AFK_WorldCellGenParam,
                AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>&)>(afk_generateWorldCells),
            100);

    /* Set up the landscape shader. */
    landscape_shaderProgram = new AFK_ShaderProgram();
    *landscape_shaderProgram << "landscape_fragment" << "landscape_geometry" << "landscape_vertex";
    landscape_shaderProgram->Link();

    landscape_shaderLight = new AFK_ShaderLight(landscape_shaderProgram->program);

    landscape_clipTransformLocation = glGetUniformLocation(landscape_shaderProgram->program, "ClipTransform");
    landscape_yCellMinLocation = glGetUniformLocation(landscape_shaderProgram->program, "yCellMin");
    landscape_yCellMaxLocation = glGetUniformLocation(landscape_shaderProgram->program, "yCellMax");

    entity_shaderProgram = new AFK_ShaderProgram();
    /* TODO How much stuff will I need here ? */
    *entity_shaderProgram << "vcol_phong_fragment" << "vcol_phong_vertex";
    entity_shaderProgram->Link();

    entity_shaderLight = new AFK_ShaderLight(entity_shaderProgram->program);

    entity_worldTransformLocation = glGetUniformLocation(entity_shaderProgram->program, "WorldTransform");
    entity_clipTransformLocation = glGetUniformLocation(entity_shaderProgram->program, "ClipTransform");

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    tilesQueued.store(0);
    tilesResumed.store(0);
    tilesComputed.store(0);
    entitiesQueued.store(0);
    entitiesMoved.store(0);
    threadEscapes.store(0);
}

AFK_World::~AFK_World()
{
    /* Delete the generator gang first, so that I know that
     * any worker threads have stopped before I start deleting
     * the things they're using.
     */
    delete genGang;

    delete landscapeCache;
    delete worldCache;

    delete landscape_shaderProgram;
    delete landscape_shaderLight;

    delete entity_shaderProgram;
    delete entity_shaderLight;
}

void AFK_World::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<long long>& modifier,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell = afk_cell(afk_vec4<long long>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[1] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[2] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));

    struct AFK_WorldCellGenParam cellParam;
    cellParam.cell                  = modifiedCell;
    cellParam.world                 = this;
    cellParam.viewerLocation        = viewerLocation;
    cellParam.camera                = &camera;
    cellParam.flags                 = 0;
    cellParam.dependency            = NULL;
    (*genGang) << cellParam;
}

void AFK_World::flipRenderQueues(void)
{
    /* Verify that the concurrency control business has done
     * its job correctly.
     */
    if (!genGang->assertNoQueuedWork())
        threadEscapes.fetch_add(1);

    landscapeRenderQueue.flipQueues();
    entityRenderQueue.flipQueues();
}

void AFK_World::alterDetail(float adjustment)
{
    /* TODO Better way of stopping afk from hogging the entirety
     * of system memory?
     * :(
     */
    float adj = std::max(std::min(adjustment, 1.2f), 0.85f);

    if (adj > 1.0f || !(worldCache->wayOutsideTargetSize() || landscapeCache->wayOutsideTargetSize()))
        detailPitch = detailPitch * adjustment;

    renderDetailPitch = detailPitch - fmod(detailPitch, 16.0f);
}

boost::unique_future<bool> AFK_World::updateWorld(void)
{
    /* Maintenance. */
    worldCache->doEvictionIfNecessary();
    landscapeCache->doEvictionIfNecessary();

    /* First, transform the protagonist location and its facing
     * into integer cell-space.
     * TODO: This is *really* going to want arbitrary
     * precision arithmetic, eventually.
     */
    Mat4<float> protagonistTransformation = afk_core.protagonist->object.getTransformation();
    Vec4<float> hgProtagonistLocation = protagonistTransformation *
        afk_vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
    Vec3<float> protagonistLocation = afk_vec3<float>(
        hgProtagonistLocation.v[0] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[1] / hgProtagonistLocation.v[3],
        hgProtagonistLocation.v[2] / hgProtagonistLocation.v[3]);
    Vec4<long long> csProtagonistLocation = afk_vec4<long long>(
        (long long)(protagonistLocation.v[0] / minCellSize),
        (long long)(protagonistLocation.v[1] / minCellSize),
        (long long)(protagonistLocation.v[2] / minCellSize),
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
    for (parentCell = protagonistCell;
        (float)cell.coord.v[3] < maxDistance;
        cell = parentCell, parentCell = cell.parent(subdivisionFactor));

    /* Draw that cell and the other cells around it.
     * TODO Can I optimise this?  It's going to attempt
     * cells that are very very far away.  Mind you,
     * that might be okay.
     */
    for (long long i = -1; i <= 1; ++i)
        for (long long j = -1; j <= 1; ++j)
            for (long long k = -1; k <= 1; ++k)
                enqueueSubcells(cell, afk_vec3<long long>(i, j, k), protagonistLocation, *(afk_core.camera));

    return genGang->start();
}

void AFK_World::display(const Mat4<float>& projection, const AFK_Light &globalLight)
{
    /* Render the landscape */
    AFK_DisplayedLandscapeTile *dlt;
    while (landscapeRenderQueue.draw_pop(dlt))
    {
        dlt->initGL();
        dlt->display(
            landscape_shaderProgram,
            landscape_shaderLight,
            globalLight,
            landscape_clipTransformLocation,
            landscape_yCellMinLocation,
            landscape_yCellMaxLocation,
            projection);
        delete dlt;
    }

    /* Render the entities */
    AFK_DisplayedEntity *de;
    while (entityRenderQueue.draw_pop(de))
    {
        de->initGL();
        de->display(
            entity_shaderProgram,
            entity_shaderLight,
            globalLight,
            entity_worldTransformLocation,
            entity_clipTransformLocation,
            projection);
        delete de;
    }
}

/* Worker for the below. */
static float toRatePerSecond(unsigned long long quantity, boost::posix_time::time_duration& interval)
{
    return (float)quantity * 1000.0f / (float)interval.total_milliseconds();
}

#define PRINT_RATE_AND_RESET(s, v) std::cout << s << toRatePerSecond((v).load(), timeSinceLastCheckpoint) << "/second" << std::endl; \
    (v).store(0);

void AFK_World::checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint)
{
    std::cout << "Detail pitch:             " << detailPitch << std::endl;
    PRINT_RATE_AND_RESET("Cells found invisible:    ", cellsInvisible)
    PRINT_RATE_AND_RESET("Tiles queued:             ", tilesQueued)
    PRINT_RATE_AND_RESET("Tiles resumed:            ", tilesResumed)
    PRINT_RATE_AND_RESET("Tiles computed:           ", tilesComputed)
    PRINT_RATE_AND_RESET("Entities queued:          ", entitiesQueued)
    PRINT_RATE_AND_RESET("Entities moved:           ", entitiesMoved)
    std::cout <<         "Cumulative thread escapes:" << threadEscapes.load() << std::endl;
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
    worldCache->printStats(ss, "World cache");
    landscapeCache->printStats(ss, "Landscape cache");
}

