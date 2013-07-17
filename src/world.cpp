/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
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
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
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
        (renderTerrain || resume) ? AFK_CLT_NONEXCLUSIVE : AFK_CLT_EXCLUSIVE))
    {
        retval = world->generateClaimedWorldCell(
            worldCell, threadId, param, queue);
        worldCell.release(threadId);
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

            world->dependenciesFollowed.fetch_add(1);
        }
    }

    return retval;
}


/* AFK_World implementation */

bool AFK_World::generateClaimedLandscapeTile(
    const AFK_Tile& tile,
    AFK_LandscapeTile& landscapeTile,
    bool displayTerrain,
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
{
    /* A LandscapeTile has several stages of creation.
     * First, make sure it's got a terrain descriptor, which
     * will tell us what terrain features go here.
     */
    landscapeTile.makeTerrainDescriptor(
        pointSubdivisionFactor,
        subdivisionFactor,
        tile,
        minCellSize,
        NULL /* Put a debug colour here */ );

    if (displayTerrain)
    {
        /* Next, create the terrain list, which is composed out of
         * the terrain descriptor for this landscape tile, and
         * those of all the parent tiles (so that child tiles
         * just add detail to an existing terrain).
         */
        AFK_TerrainList terrainList;
        std::vector<AFK_Tile> missing;
        missing.reserve(8); /* Stop it from doing lots of small realloc's */
        landscapeTile.buildTerrainList(
            terrainList,
            missing,
            tile,
            subdivisionFactor,
            maxDistance,
            landscapeCache);

        if (missing.size() > 0)
        {
            /* Uh oh!  Some of the tiles required to render this
             * terrain are not in the cache.  I need to enqueue
             * tasks to compute those missing cells, then resume
             * this one after.
             */
            struct AFK_WorldCellGenDependency *dependency = new struct AFK_WorldCellGenDependency();
            dependency->count.store(missing.size());
            dependency->finalCell = new struct AFK_WorldCellGenParam(param);
            dependency->finalCell->flags |= AFK_WCG_FLAG_RESUME;
            if (param.dependency)
            { /* I can't complete my own dependency until my
                 * resume has finished
                 */
                param.dependency->count.fetch_add(1);
            }

            /* To make the async gang properly render these
             * landscape tiles and come back, I'll enqueue all
             * matching cells with the render flag, which
             * means, render the landscape only, don't try to
             * recurse for cells to display.
             */
            for (std::vector<AFK_Tile>::iterator missingIt = missing.begin();
                missingIt != missing.end(); ++missingIt)
            {
                struct AFK_WorldCellGenParam mcParam;
                mcParam.cell                = afk_cell(*missingIt, 0);
                mcParam.world               = param.world;
                mcParam.viewerLocation      = param.viewerLocation;
                mcParam.camera              = param.camera;
                mcParam.flags               = AFK_WCG_FLAG_TERRAIN_RENDER;
                mcParam.dependency          = dependency;
                queue.push(mcParam);
            }
    
            tilesFoundMissing.fetch_add(missing.size());
        }
        else
        {
            if (landscapeTile.hasRawTerrain(tile, pointSubdivisionFactor))
                landscapeTile.computeGeometry(pointSubdivisionFactor, tile, terrainList);
        }
    }

    return true;
}

bool AFK_World::generateClaimedWorldCell(
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
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
    }
    else
    {
        /* TODO: Non-landscape stuff goes here.  :-) */

        /* Find the tile where any landscape at this cell would be
         * homed
         */
        AFK_Tile tile = afk_tile(cell);

        /* We display landscape at a cell if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell
         */
        bool displayTerrain = (cell.coord.v[3] == MIN_CELL_PITCH ||
            worldCell.testDetailPitch(detailPitch, *camera, viewerLocation));

        /* If we want to display the landscape here, OR we've been
         * called with the flag that makes us render it regardless,
         * look up the landscape tile in the landscape cache.
         */
        if (displayTerrain || renderTerrain)
        {
            AFK_LandscapeTile& landscapeTile = (*landscapeCache)[tile];
            if (landscapeTile.claimYieldLoop(threadId, AFK_CLT_NONEXCLUSIVE))
            {
                /* Make sure the terrain has been properly generated
                 * in this landscape tile.
                 */
                retval = generateClaimedLandscapeTile(
                    tile, landscapeTile, displayTerrain, threadId, param, queue);

                if (displayTerrain && landscapeTile.hasGeometry())
                {
                    /* Get it to make us a DisplayedLandscapeTile to
                     * feed into the display queue.
                     */
                    AFK_DisplayedLandscapeTile *dptr =
                        landscapeTile.makeDisplayedLandscapeTile(cell, minCellSize);

                    if (dptr)
                    {
                        landscapeRenderQueue.update_push(dptr);
                        cellsQueued.fetch_add(1);
                    }
                }

                landscapeTile.release(threadId);
            }
        }

        /* If the terrain here was at too coarse a resolution to
         * be displayable, recurse through the subcells
         */
        if (!displayTerrain && !renderTerrain && someVisible && !resume)
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
    float startingDetailPitch):
        detailPitch(startingDetailPitch), /* This is a starting point */
        landscapeRenderQueue(100), /* TODO make a better guess */
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
        24, 6, AFK_HashCell(), 100000, 0xfffffffeu);

    landscapeCache = new AFK_LANDSCAPE_CACHE(
        22, 6, AFK_HashTile(), 20000, 0xfffffffdu);

    genGang = new AFK_AsyncGang<struct AFK_WorldCellGenParam, bool>(
            boost::function<bool (unsigned int, const struct AFK_WorldCellGenParam,
                ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)&)>(afk_generateWorldCells),
            100);

    /* Set up the world shader. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "landscape_fragment" << "landscape_geometry" << "landscape_vertex";
    //*shaderProgram << "vcol_phong_fragment" << "vcol_phong_vertex";
    shaderProgram->Link();

    shaderLight = new AFK_ShaderLight(shaderProgram->program);

    clipTransformLocation = glGetUniformLocation(shaderProgram->program, "ClipTransform");
    yCellMinLocation = glGetUniformLocation(shaderProgram->program, "yCellMin");
    yCellMaxLocation = glGetUniformLocation(shaderProgram->program, "yCellMax");

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsGenerated.store(0);
    tilesFoundMissing.store(0);
    dependenciesFollowed.store(0);
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
    delete shaderProgram;
    delete shaderLight;
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
    landscapeRenderQueue.flipQueues();
}

void AFK_World::alterDetail(float adjustment)
{
    /* TODO Better way of stopping afk from hogging the entirety
     * of system memory?
     * :(
     */
    float adj = std::max(std::min(adjustment, 2.0f), 0.5f);

    if (adj > 1.0f || !(worldCache->wayOutsideTargetSize() || landscapeCache->wayOutsideTargetSize()))
        detailPitch = detailPitch * adjustment;
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
            shaderProgram,
            shaderLight,
            globalLight,
            clipTransformLocation,
            yCellMinLocation,
            yCellMaxLocation,
            projection);
        delete dlt;
    }
}

/* Worker for the below. */
static float toRatePerSecond(unsigned long long quantity, boost::posix_time::time_duration& interval)
{
    return (float)quantity * 1000.0f / (float)interval.total_milliseconds();
}

void AFK_World::checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint)
{
    std::cout << "Detail pitch:             " << detailPitch << std::endl;
    std::cout << "Cells found invisible:    " << toRatePerSecond(cellsInvisible.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells queued:             " << toRatePerSecond(cellsQueued.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells generated:          " << toRatePerSecond(cellsGenerated.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Tiles found missing:      " << toRatePerSecond(tilesFoundMissing.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Dependencies followed:    " << toRatePerSecond(dependenciesFollowed.load(), timeSinceLastCheckpoint) << "/second" << std::endl;

    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsGenerated.store(0);
    tilesFoundMissing.store(0);
    dependenciesFollowed.store(0);
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
    worldCache->printStats(ss, "World cache");
    landscapeCache->printStats(ss, "Landscape cache");
}

