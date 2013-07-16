/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "world.hpp"

/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1

#define SPILL_DEBUG 0


/* The spill helper */

void afk_spillHelper(
    unsigned int threadId,
    const AFK_Cell& spillCell,
    boost::shared_ptr<AFK_DWC_INDEX_BUF> spillIndices,
    const AFK_Cell& sourceCell,
    const AFK_DisplayedWorldCell& sourceDWC,
    AFK_World *world)
{
    if (spillCell.coord.v[1] == 0)
    {
        std::ostringstream ss;
        ss << "Tried to spill to y=0 cell " << spillCell;
        throw AFK_Exception(ss.str());
    }

    AFK_DisplayedWorldCell& spillDWC = (*(world->dwcCache))[spillCell];
    if (spillDWC.claimYieldLoop(threadId, true))
    {
        spillDWC.spill(sourceDWC, sourceCell, spillIndices);
#if SPILL_DEBUG
        AFK_DEBUG_PRINT(spillCell << ", ")
#endif
        world->cellsSpilledTo.fetch_add(1);
        spillDWC.release(threadId);
    }
    else
    {
        world->spillCellsUnclaimed.fetch_add(1);
    }
}


/* The cell generating worker */

#define ENQUEUE_DEBUG_COLOURS 0


/* The AFK_WorldCellGenParam flags. */
#define AFK_WCG_FLAG_ENTIRELY_VISIBLE   2 /* Cell is already known to be entirely within the viewing frustum */
#define AFK_WCG_FLAG_TERRAIN_RENDER     4 /* Render the terrain regardless of visibility or LoD */
#define AFK_WCG_FLAG_RESUME             8 /* This is a resume after dependent cells were computed */

bool afk_generateClaimedDWC(
    AFK_WorldCell& worldCell,
    AFK_DisplayedWorldCell& displayed,
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
{
    const AFK_Cell& cell                = param.cell;
    AFK_World *world                    = param.world;
    const Vec3<float>& viewerLocation   = param.viewerLocation;
    const AFK_Camera *camera            = param.camera;

    /* If this cell is at y=0 (the `terrain-owning' cell layer)... */
    if (cell.coord.v[1] == 0)
    {
        /* Make sure its terrain has been fully computed */
        if (displayed.hasRawTerrain(worldCell.getRealCoord(), world->pointSubdivisionFactor))
        {
            /* Get this cell's terrain list. */
            AFK_TerrainList terrainList;
            std::vector<AFK_Cell> missing;
            missing.reserve(8);
            worldCell.buildTerrainList(terrainList, missing, world->maxDistance, world->worldCache);

            if (missing.size() > 0)
            {
                /* Uh oh.  I need to enqueue all of those missing cells,
                 * then resume this one in order to compute and spill the
                 * final geometry.
                 */
                struct AFK_WorldCellGenDependency *dependency = new struct AFK_WorldCellGenDependency();
                dependency->count.store(missing.size() - 1);
                dependency->finalCell = new struct AFK_WorldCellGenParam(param);
                dependency->finalCell->flags |= AFK_WCG_FLAG_RESUME;
                if (param.dependency)
                { /* I can't complete my own dependency until my
                     * resume has finished
                     */
                    param.dependency->count.fetch_add(1);
                }

                for (std::vector<AFK_Cell>::iterator missingIt = missing.begin();
                    missingIt != missing.end(); ++missingIt)
                {
                    struct AFK_WorldCellGenParam mcParam;
                    mcParam.cell                = *missingIt;
                    mcParam.world               = param.world;
                    mcParam.viewerLocation      = param.viewerLocation;
                    mcParam.camera              = param.camera;
                    mcParam.flags               = AFK_WCG_FLAG_TERRAIN_RENDER;
                    mcParam.dependency          = dependency;
                    queue.push(mcParam);
                }

                world->cellsFoundMissing.fetch_add(missing.size());
            }
            else
            {
                /* With that, I can make the geometry. */
                displayed.computeGeometry(
                    world->pointSubdivisionFactor, worldCell.getCell(), world->minCellSize, terrainList);
                world->cellsGenerated.fetch_add(1);
            }
        }

        /* Make sure its geometry is spilled into any other cells
         * that we might have in the cache
         */
        std::vector<AFK_Cell>::iterator spillCellsIt = displayed.spillCellsBegin();
        std::vector<boost::shared_ptr<AFK_DWC_INDEX_BUF> >::iterator spillIsIt = displayed.spillIsBegin();

#if SPILL_DEBUG
        AFK_DEBUG_PRINT("Spilling " << worldCell.getCell() << " -> ")
#endif

        while (spillCellsIt != displayed.spillCellsEnd() &&
            spillIsIt != displayed.spillIsEnd())
        {
            afk_spillHelper(threadId, *spillCellsIt, *spillIsIt,
                cell, displayed, world);
            
            ++spillCellsIt;
            ++spillIsIt;
        }
#if SPILL_DEBUG
        AFK_DEBUG_PRINTL("")
#endif
    }
    else
    {
        if (!displayed.hasGeometry())
        {
            /* Oh dear.  We need to request a render of the terrain-owning
             * cell's geometry.
             * We don't need to request a resume, though -- the zero cell
             * will spill the correct geometry into this one, and we'll
             * already have enqueued it for rendering (below).
             */
            struct AFK_WorldCellGenParam zeroCellParam;
            zeroCellParam.cell              = worldCell.terrainRoot();
            zeroCellParam.world             = world;
            zeroCellParam.viewerLocation    = viewerLocation;
            zeroCellParam.camera            = camera;
            zeroCellParam.flags             = AFK_WCG_FLAG_TERRAIN_RENDER;
            zeroCellParam.dependency        = NULL;
            queue.push(zeroCellParam);

            world->zeroCellsRequested.fetch_add(1);
        }
    }

    return true;
}

bool afk_generateClaimedWorldCell(
    AFK_WorldCell& worldCell,
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
{
    const AFK_Cell& cell                = param.cell;
    AFK_World *world                    = param.world;
    const Vec3<float>& viewerLocation   = param.viewerLocation;
    const AFK_Camera *camera            = param.camera;

    bool entirelyVisible                = ((param.flags & AFK_WCG_FLAG_ENTIRELY_VISIBLE) != 0);
    bool renderTerrain                  = ((param.flags & AFK_WCG_FLAG_TERRAIN_RENDER) != 0);
    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval = true;

    worldCell.bind(cell, world->minCellSize);

    /* Check for visibility. */
    bool someVisible = entirelyVisible;
    bool allVisible = entirelyVisible;

    if (!entirelyVisible) worldCell.testVisibility(*camera, someVisible, allVisible);
    if (!someVisible && !renderTerrain)
    {
        world->cellsInvisible.fetch_add(1);

        /* Nothing else to do with it now either. */
    }
    else
    {
        Vec3<float> *debugColour = NULL;

#if ENQUEUE_DEBUG_COLOURS
        /* Here's an LoD one... (apparently) */
#if 0
        Vec3<float> tint =
            cell.coord.v[3] == MIN_CELL_PITCH ? Vec3<float>(0.0f, 0.0f, 1.0f) :
            cell.coord.v[3] == MIN_CELL_PITCH * subdivisionFactor ? Vec3<float>(0.0f, 1.0f, 0.0f) :
            Vec3<float>(1.0f, 0.0f, 0.0f);
#else
        /* And here's a location one... */
        Vec3<float> tint(
            0.0f,
            fmod(realCell.coord.v[0], 16.0),
            fmod(realCell.coord.v[2], 16.0));
#endif
        debugColour = &tint;
#endif /* ENQUEUE_DEBUG_COLOURS */
        /* Make sure there's terrain here. */
        worldCell.makeTerrain(
            world->pointSubdivisionFactor,
            world->subdivisionFactor,
            world->minCellSize,
            debugColour);

        /* We display a cell's terrain if its detail pitch is at the
         * target detail pitch, or if it's already the smallest
         * possible cell
         */
        bool displayTerrain = (cell.coord.v[3] == MIN_CELL_PITCH ||
            worldCell.testDetailPitch(world->detailPitch, *camera, viewerLocation));

        /* Dig up the displayed cell for this terrain */
        if (displayTerrain || renderTerrain)
        {
            AFK_DisplayedWorldCell& displayed = (*(world->dwcCache))[cell];
            if (displayed.claimYieldLoop(threadId, !resume))
            {
                retval = afk_generateClaimedDWC(
                    worldCell, displayed, threadId, param, queue);
                if (displayTerrain)
                {
                    /* It goes into the display queue */
                    AFK_DisplayedWorldCell *dptr = &displayed;
                    world->renderQueue.update_push(dptr);
                    world->cellsQueued.fetch_add(1);
                }
                displayed.release(threadId);
            }
        }

        /* If the terrain here was at too coarse a resolution to
         * be displayable, recurse through the subcells
         */
        if (!displayTerrain && someVisible && !resume)
        {
            size_t subcellsSize = CUBE(world->subdivisionFactor);
            AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow.  Maybe make it an iterator */
            unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize);

            if (subcellsCount == subcellsSize)
            {
                for (unsigned int i = 0; i < subcellsCount; ++i)
                {
                    struct AFK_WorldCellGenParam subcellParam;
                    subcellParam.cell               = subcells[i];
                    subcellParam.world              = world;
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

bool afk_generateWorldCells(
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
{
    const AFK_Cell& cell                = param.cell;
    AFK_World *world                    = param.world;

    bool resume                         = ((param.flags & AFK_WCG_FLAG_RESUME) != 0);

    bool retval;

    AFK_WorldCell& worldCell = (*(world->worldCache))[cell];
    if (worldCell.claimYieldLoop(threadId, !resume))
    {
        retval = afk_generateClaimedWorldCell(
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

AFK_World::AFK_World(
    float _maxDistance,
    unsigned int _subdivisionFactor,
    unsigned int _pointSubdivisionFactor,
    float _minCellSize,
    float startingDetailPitch):
        detailPitch(startingDetailPitch), /* This is a starting point */
        renderQueue(100), /* TODO make a better guess */
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

    dwcCache = new AFK_DWC_CACHE(
        22, 6, AFK_HashCell(), 20000, 0xfffffffdu);

    genGang = new AFK_AsyncGang<struct AFK_WorldCellGenParam, bool>(
            boost::function<bool (unsigned int, const struct AFK_WorldCellGenParam,
                ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)&)>(afk_generateWorldCells),
            100);

    /* Set up the world shader. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "vcol_phong_fragment" << "vcol_phong_vertex";
    shaderProgram->Link();

    worldTransformLocation = glGetUniformLocation(shaderProgram->program, "WorldTransform");
    clipTransformLocation = glGetUniformLocation(shaderProgram->program, "ClipTransform");

    /* Initialise the statistics. */
    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsGenerated.store(0);
    cellsFoundMissing.store(0);
    cellsSpilledTo.store(0);
    spillCellsUnclaimed.store(0);
    zeroCellsRequested.store(0);
    dependenciesFollowed.store(0);
}

AFK_World::~AFK_World()
{
    /* Delete the generator gang first, so that I know that
     * any worker threads have stopped before I start deleting
     * the things they're using.
     */
    delete genGang;

    delete dwcCache;
    delete worldCache;
    delete shaderProgram;
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
    renderQueue.flipQueues();
}

void AFK_World::alterDetail(float adjustment)
{
    /* TODO Better way of stopping afk from hogging the entirety
     * of system memory?
     * :(
     */
    float adj = std::max(std::min(adjustment, 2.0f), 0.5f);

    if (adj > 1.0f || !(worldCache->wayOutsideTargetSize() || dwcCache->wayOutsideTargetSize()))
        detailPitch = detailPitch * adjustment;
}

boost::unique_future<bool> AFK_World::updateLandMap(void)
{
    /* Maintenance. */
    worldCache->doEvictionIfNecessary();
    dwcCache->doEvictionIfNecessary();

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
        cell = parentCell, parentCell = cell.parent());

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

void AFK_World::display(const Mat4<float>& projection)
{
    AFK_DisplayedWorldCell *displayedCell;
    bool first;
    while (renderQueue.draw_pop(displayedCell))
    {
        /* All cells are the same in many ways, so I can do this
         * just the once
         * TODO: I really ought to move this out into a drawable
         * single object related to AFK_World instead
         */
        if (first)
        {
            displayedCell->displaySetup(projection);
            first = false;
        }

        displayedCell->display(projection);
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
    std::cout << "Cells found missing:      " << toRatePerSecond(cellsFoundMissing.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells spilled to:         " << toRatePerSecond(cellsSpilledTo.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Spill cells unclaimed:    " << toRatePerSecond(spillCellsUnclaimed.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells at y=0 requested:   " << toRatePerSecond(zeroCellsRequested.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Dependencies followed:    " << toRatePerSecond(dependenciesFollowed.load(), timeSinceLastCheckpoint) << "/second" << std::endl;

    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsGenerated.store(0);
    cellsFoundMissing.store(0);
    cellsSpilledTo.store(0);
    spillCellsUnclaimed.store(0);
    zeroCellsRequested.store(0);
    dependenciesFollowed.store(0);
}

void AFK_World::printCacheStats(std::ostream& ss, const std::string& prefix)
{
    worldCache->printStats(ss, "World cache");
    dwcCache->printStats(ss, "DWC cache");
}

