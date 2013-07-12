/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include "core.hpp"
#include "exception.hpp"
#include "world.hpp"

/* TODO remove debug?  (or something) */
#define PROTAGONIST_CELL_DEBUG 1



/* The cell generating worker */


#define ENQUEUE_DEBUG_COLOURS 0

bool afk_generateWorldCells(
    struct AFK_WorldCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue)
{
    unsigned int recCount               = param.recCount;
    const AFK_Cell& cell                = param.cell;
    bool topLevel                       = param.topLevel;
    AFK_World *world            = param.world;
    const Vec3<float>& viewerLocation   = param.viewerLocation;
    const AFK_Camera *camera            = param.camera;
    bool entirelyVisible                = param.entirelyVisible; 

    AFK_WorldCell& worldCell = (*(world->cache))[cell];
    if (!worldCell.claim()) return true;
    worldCell.bind(cell, topLevel, world->minCellSize);

    /* Is there any terrain here?
     * TODO: If there isn't, I still want to evaluate
     * contained objects, and any terrain geometry that
     * might have "bled" into this cell.  But for now,
     * just dump it.
     */
    if (cell.coord.v[1] != 0)
    {
        world->cellsEmpty.fetch_add(1);
    }
    else
    {
        /* Check for visibility. */
        bool someVisible = false;
        bool allVisible = entirelyVisible;

        worldCell.testVisibility(*camera, someVisible, allVisible);
        if (!someVisible)
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

            /* If it's the smallest possible cell, or its detail pitch
             * is at the target detail pitch, include it as-is
             */
            if (cell.coord.v[3] == MIN_CELL_PITCH ||
                worldCell.testDetailPitch(world->detailPitch, *camera, viewerLocation))
            {
                /* If the displayed cell isn't there already, better make it now.
                 * TODO Expensive: enqueue as separate work item (maybe even in OpenCL?)
                 */
                if (!worldCell.displayed)
                {
                    worldCell.displayed = new AFK_DisplayedWorldCell(
                        &worldCell,
                        world->pointSubdivisionFactor,
                        world->cache);
                    world->cellsGenerated.fetch_add(1);
                }
                else
                {
                    world->cellsCached.fetch_add(1);
                }
        
                /* Now, push it into the queue as well */
                world->renderQueue.update_push(worldCell.displayed);
                world->cellsQueued.fetch_add(1);
            }
            else
            {
                /* Recurse through the subcells. */
                size_t subcellsSize = CUBE(world->subdivisionFactor);
                AFK_Cell *subcells = new AFK_Cell[subcellsSize]; /* TODO avoid heap thrashing somehow.  Maybe make it an iterator */
                unsigned int subcellsCount = cell.subdivide(subcells, subcellsSize);

                if (subcellsCount == subcellsSize)
                {
                    for (unsigned int i = 0; i < subcellsCount; ++i)
                    {
                        struct AFK_WorldCellGenParam subcellParam;
                        subcellParam.recCount           = recCount + 1;
                        subcellParam.cell               = subcells[i];
                        subcellParam.topLevel           = false;
                        subcellParam.world          = world;
                        subcellParam.viewerLocation     = viewerLocation;
                        subcellParam.camera             = camera;
                        subcellParam.entirelyVisible    = allVisible;

#if AFK_NO_THREADING
                        afk_generateWorldCells(subcellParam, queue);
#else
                        if (subcellParam.recCount == world->recursionsPerTask)
                        {
                            /* I've hit the task recursion limit, queue this as a new task */
                            subcellParam.recCount = 0;
                            queue.push(subcellParam);
                        }
                        else
                        {
                            /* Use a direct function call */
                            afk_generateWorldCells(subcellParam, queue);
                        }
#endif
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
    }

    return true;
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
#if AFK_NO_THREADING
        fakeQueue(100),
        fakePromise(NULL),
#endif
        /* On HyperThreading systems, can ignore the hyper threads:
         * there appears to be zero benefit.  However, I can't figure
         * out how to identify this.
         */
        recursionsPerTask(1), /* This seems to do better than higher numbers */
        maxDistance(_maxDistance),
        subdivisionFactor(_subdivisionFactor),
        pointSubdivisionFactor(_pointSubdivisionFactor),
        minCellSize(_minCellSize)
{
    /* Set up the cache and generator gang. */
    cache = new AFK_WorldCache(40000 /* target cache size.  TODO make based on system/GPU memory
                                      * Observation: 20000 uses about 7% memory on guinevere?
                                      * 22 hashbits are good for 20000, 23 for 40000? */);

    genGang = new AFK_AsyncGang<struct AFK_WorldCellGenParam, bool>(
            boost::function<bool (const struct AFK_WorldCellGenParam,
                ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)&)>(afk_generateWorldCells),
            100);

    /* Set up the world shader. */
    shaderProgram = new AFK_ShaderProgram();
    *shaderProgram << "vcol_phong_fragment" << "vcol_phong_vertex";
    shaderProgram->Link();

    worldTransformLocation = glGetUniformLocation(shaderProgram->program, "WorldTransform");
    clipTransformLocation = glGetUniformLocation(shaderProgram->program, "ClipTransform");

    /* Initialise the statistics. */
    cellsEmpty.store(0);
    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsCached.store(0);
    cellsGenerated.store(0);
}

AFK_World::~AFK_World()
{
    /* Delete the generator gang first, so that I know that
     * any worker threads have stopped before I start deleting
     * the things they're using.
     */
    delete genGang;

    delete cache;
    delete shaderProgram;
#if AFK_NO_THREADING
    if (fakePromise) delete fakePromise;
#endif
}

void AFK_World::enqueueSubcells(
    const AFK_Cell& cell,
    const Vec3<long long>& modifier,
    const Vec3<float>& viewerLocation,
    const AFK_Camera& camera)
{
    AFK_Cell modifiedCell = afk_cell(afk_vec4<long long>(
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[0],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[1],
        cell.coord.v[0] + cell.coord.v[3] * modifier.v[2],
        cell.coord.v[3]));

    struct AFK_WorldCellGenParam cellParam;
    cellParam.recCount              = 0;
    cellParam.cell                  = modifiedCell;
    cellParam.topLevel              = true;
    cellParam.world             = this;
    cellParam.viewerLocation        = viewerLocation;
    cellParam.camera                = &camera;
    cellParam.entirelyVisible       = false;

#if AFK_NO_THREADING
    afk_generateWorldCells(cellParam, fakeQueue);
#else
    (*genGang) << cellParam;
#endif
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

    if (adj > 1.0f || cache->withinTargetSize())
        //detailPitch = std::min((double)detailPitch * adjustment, 128.0d);
        detailPitch = detailPitch * adjustment;
}

boost::unique_future<bool> AFK_World::updateLandMap(void)
{
    /* Maintenance. */
    cache->doEvictionIfNecessary();

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

#if AFK_NO_THREADING
    if (fakePromise) delete fakePromise;
    fakePromise = new boost::promise<bool>();
    fakePromise->set_value(true);
    return fakePromise->get_future();
#else
    return genGang->start();
#endif
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
    std::cout << "Cells found empty:        " << toRatePerSecond(cellsEmpty.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells found invisible:    " << toRatePerSecond(cellsInvisible.load(), timeSinceLastCheckpoint) << "/second" << std::endl;
    std::cout << "Cells queued:             " << toRatePerSecond(cellsQueued.load(), timeSinceLastCheckpoint) << "/second (" << (100 * cellsCached.load() / cellsQueued.load()) << "\% cached)" << std::endl;
    std::cout << "Cells generated:          " << toRatePerSecond(cellsGenerated.load(), timeSinceLastCheckpoint) << "/second" << std::endl;

    cellsEmpty.store(0);
    cellsInvisible.store(0);
    cellsQueued.store(0);
    cellsCached.store(0);
    cellsGenerated.store(0);
}

