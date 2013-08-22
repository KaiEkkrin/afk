/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_H_
#define _AFK_WORLD_H_

#include "afk.hpp"

#include <sstream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/function.hpp>
#include <boost/thread/thread.hpp>

#include "async/async.hpp"
#include "async/work_queue.hpp"
#include "camera.hpp"
#include "cell.hpp"
#include "computer.hpp"
#include "config.hpp"
#include "data/evictable_cache.hpp"
#include "data/fair.hpp"
#include "data/moving_average.hpp"
#include "data/stage_timer.hpp"
#include "def.hpp"
#include "entity.hpp"
#include "entity_display_queue.hpp"
#include "jigsaw.hpp"
#include "landscape_display_queue.hpp"
#include "landscape_tile.hpp"
#include "shader.hpp"
#include "shape.hpp"
#include "shrinkform_base.hpp"
#include "shrinkform_compute_queue.hpp"
#include "terrain_base_tile.hpp"
#include "terrain_compute_queue.hpp"
#include "tile.hpp"
#include "world_cell.hpp"

/* The world of AFK. */

class AFK_LandscapeDisplayQueue;
class AFK_LandscapeTile;
class AFK_World;

struct AFK_WorldCellGenParam;


/* This structure describes a cell generation dependency.
 * Every time a cell generating worker gets a parameter with
 * one of these, it decrements `count'.  The worker that
 * decrements `count' to zero enqueues the final cell and
 * deletes this structure.
 */
struct AFK_WorldCellGenDependency
{
    boost::atomic<unsigned int> count;
    struct AFK_WorldCellGenParam *finalCell;
};

/* The parameter for the cell generating worker.
 */
struct AFK_WorldCellGenParam
{
    AFK_Cell cell;
    AFK_World *world;
    Vec3<float> viewerLocation;
    const AFK_Camera *camera;
    unsigned int flags;
    struct AFK_WorldCellGenDependency *dependency;
};

/* This is the cell generating worker function */
bool afk_generateWorldCells(
    unsigned int threadId,
    struct AFK_WorldCellGenParam param,
    AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue);

/* This is the world.  AFK_Core will have one of these.
 */
class AFK_World
{
protected:
    /* These derived results are things I want to be carefully
     * managed now that I'm trying to adjust the LOD on the fly.
     */

    /* The maximum number of pixels each detail point on the
     * world should occupy on screen.  Determines when
     * to split a cell into smaller ones.
     * (I'm expecting to have to number-crunch a lot with this
     * value to get the right number to pass to the points
     * generator, but hopefully I'll only have to do that
     * once per world cell)
     */
    const float startingDetailPitch;
    const float maxDetailPitch;
    float detailPitch;

    /* Let's try averaging it for the render.
     */
    AFK_MovingAverage<float> averageDetailPitch;

    /* Gather statistics.  (Useful.)
     */
    boost::atomic<unsigned long long> cellsInvisible;
    boost::atomic<unsigned long long> tilesQueued;
    boost::atomic<unsigned long long> tilesResumed;
    boost::atomic<unsigned long long> tilesComputed;
    boost::atomic<unsigned long long> tilesRecomputedAfterSweep;
    boost::atomic<unsigned long long> entitiesQueued;
    boost::atomic<unsigned long long> entitiesMoved;
    boost::atomic<unsigned long long> threadEscapes;

    /* Landscape shader details. */
    AFK_ShaderProgram *landscape_shaderProgram;
    AFK_ShaderLight *landscape_shaderLight;
    GLuint landscape_clipTransformLocation;

    /* Entity shader details. */
    AFK_ShaderProgram *entity_shaderProgram;
    AFK_ShaderLight *entity_shaderLight;
    GLuint entity_projectionTransformLocation;

    /* The cache of world cells we're tracking.
     */
#ifndef AFK_WORLD_CACHE
#define AFK_WORLD_CACHE AFK_EvictableCache<AFK_Cell, AFK_WorldCell, AFK_HashCell>
#endif
    AFK_WORLD_CACHE *worldCache;

    /* These jigsaws form the computed landscape artwork. */
    AFK_JigsawCollection *landscapeJigsaws;

    /* The cache of landscape cells we're tracking.
     * TODO: I think at some point, all of this should
     * move to a separate `landscape' module (one per
     * world) that handles the landscape, otherwise
     * I'm going to get a bit of overload?
     */
#ifndef AFK_LANDSCAPE_CACHE
#define AFK_LANDSCAPE_CACHE AFK_EvictableCache<AFK_Tile, AFK_LandscapeTile, AFK_HashTile>
#endif
    AFK_LANDSCAPE_CACHE *landscapeCache;
    unsigned int tileCacheEntries;

    /* The terrain computation fair.  Yeah, yeah. */
    AFK_Fair<AFK_TerrainComputeQueue> landscapeComputeFair;

    /* The landscape render fair.  In jigsaw order, just
     * like the terrain compute fair, above.
     * These are transient objects -- delete them after
     * rendering.
     */
    AFK_Fair<AFK_LandscapeDisplayQueue> landscapeDisplayFair;

    /* The basic landscape tile geometry. */
    GLuint landscapeTileArray;
    AFK_TerrainBaseTile *landscapeTerrainBase;

    /* The entity render fair -- a queue of Entities to
     * display onscreen.
     */
    AFK_Fair<AFK_EntityDisplayQueue> entityDisplayFair;

    /* These jigsaws form the computed shape artwork. */
    AFK_JigsawCollection *shapeJigsaws;

    /* The cache of shapes we're tracking.
     * (It was kind of inevitable, wasn't it? :) )
     * TODO: I need some sane kind of way of indexing shapes.
     * Right now I'm just going to give them an integer label.
     */
#ifndef AFK_SHAPE_CACHE
#define AFK_SHAPE_CACHE AFK_EvictableCache<unsigned int, AFK_Shape, boost::hash<unsigned int> >
#endif
    AFK_SHAPE_CACHE *shapeCache;
    unsigned int shapeCacheEntries;

    /* The shape computation fair, for when I'm making new ones. */
    AFK_Fair<AFK_ShrinkformComputeQueue> shapeComputeFair;

    /* The basic shape geometry. */
    GLuint shrinkformBaseArray;
    AFK_ShrinkformBase *shrinkformBase;

    /* The cell generating gang */
    AFK_AsyncGang<struct AFK_WorldCellGenParam, bool> *genGang;

    /* Cell generation worker delegates. */

    /* Makes sure a landscape tile has a terrain descriptor,
     * and checks if its geometry needs generating.
     * Returns true if this thread is to generate the tile's
     * geometry, else false.
     */
    bool checkClaimedLandscapeTile(
        const AFK_Tile& tile,
        AFK_LandscapeTile& landscapeTile,
        bool display);

    /* Generates a landscape tile's geometry. */
    void generateLandscapeArtwork(
        const AFK_Tile& tile,
        AFK_LandscapeTile& landscapeTile,
        unsigned int threadId);

    /* Generates this world cell, as necessary. */
    bool generateClaimedWorldCell(
        AFK_WorldCell& worldCell,
        unsigned int threadId,
        struct AFK_WorldCellGenParam param,
        AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue);

public:
    /* Overall world parameters. */

    /* The distance to the furthest world cell to consider.
     * (You may want to use zFar, or maybe not.)
     */
    const float maxDistance;

    /* The size of the biggest cell (across one dimension) shall
     * be equal to maxDistance; this is also the furthest-
     * away cell we'll consider drawing.  From there, cells get
     * subdivided by splitting each edge this number of times --
     * thus each cell gets split into this number cubed more
     * detailed cells
     */
    const unsigned int subdivisionFactor;

    /* The size of the smallest cell. */
    const float minCellSize;

    /* These parameters define the sizes of the landscape tiles. */
    const AFK_LandscapeSizes lSizes;

    /* These parameters define the sizes of the shapes in the world. */
    const AFK_ShapeSizes sSizes;

    /* These parameters define how to initially populate a world
     * cell with entities.
     */
    const unsigned int maxEntitiesPerCell;
    const unsigned int entitySparseness;


    AFK_World(
        const AFK_Config *config,
        const AFK_Computer *computer,
        float _maxDistance,
        unsigned int worldCacheSize, /* in bytes */
        unsigned int tileCacheSize, /* also in bytes */
        unsigned int shapeCacheSize, /* likewise */
        cl_context ctxt);
    virtual ~AFK_World();

    /* Helper for the below -- requests a particular cell
     * at an offset (in cell scale multiples) from the
     * supplied one.
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const Vec3<long long>& modifier,
        const Vec3<float>& viewerLocation,
        const AFK_Camera& camera);

    /* Call when we're about to start a new frame. */
    void flipRenderQueues(cl_context ctxt, const AFK_Frame& newFrame);

    /* For changing the level of detail.  Values >1 decrease
     * it.  Values <1 increase it.
     * I'm not entirely sure what the correlation between the
     * detail pitch and the amount of processing power required
     * is, but I expect it's in the neighbourhood of
     * (1 / detailPitch ** 2) ...
     */
    void alterDetail(float adjustment);

    /* This function drives the cell generating worker to
     * update the world cache and enqueue visible
     * cells.  Returns a future that becomes available
     * when we're done.
     */
    boost::unique_future<bool> updateWorld(void);

    /* CL-tasks-at-start-of-frame function. */
    void doComputeTasks(void);

    /* Draws the world in the current OpenGL context.
     * (There's no AFK_DisplayedObject for the world.)
     */
    void display(const Mat4<float>& projection, const AFK_Light &globalLight);

    /* Takes a world checkpoint. */
    void checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint);

    void printCacheStats(std::ostream& ss, const std::string& prefix);

    friend bool afk_generateWorldCells(
        unsigned int threadId,
        struct AFK_WorldCellGenParam param,
        AFK_WorkQueue<struct AFK_WorldCellGenParam, bool>& queue);
};

#endif /* _AFK_WORLD_H_ */

