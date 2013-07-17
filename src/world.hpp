/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORLD_H_
#define _AFK_WORLD_H_

#include "afk.hpp"

#include <sstream>

#include <boost/atomic.hpp>
#include <boost/function.hpp>
#include <boost/thread/thread.hpp>

#include "async/async.hpp"
#include "camera.hpp"
#include "cell.hpp"
#include "data/evictable_cache.hpp"
#include "data/render_queue.hpp"
#include "def.hpp"
#include "displayed_landscape_tile.hpp"
#include "landscape_tile.hpp"
#include "shader.hpp"
#include "tile.hpp"
#include "world_cell.hpp"

/* The world of AFK. */

class AFK_DisplayedLandscapeTile;
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
    ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue);

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
     * TODO: Can I use this and zNear to calculate the
     * maximum number of subdivisions?  I bet I can.
     */
    float detailPitch;

    /* Gather statistics.  (Useful.)
     */
    boost::atomic<unsigned long long> cellsInvisible;
    boost::atomic<unsigned long long> cellsQueued;
    boost::atomic<unsigned long long> cellsGenerated;
    boost::atomic<unsigned long long> tilesFoundMissing;
    boost::atomic<unsigned long long> dependenciesFollowed;

    /* World shader details. */
    AFK_ShaderProgram *shaderProgram;
    AFK_ShaderLight *shaderLight;
    GLuint clipTransformLocation;
    GLuint yCellMinLocation;
    GLuint yCellMaxLocation;

    /* The cache of world cells we're tracking.
     */
#ifndef AFK_WORLD_CACHE
#define AFK_WORLD_CACHE AFK_EvictableCache<AFK_Cell, AFK_WorldCell, AFK_HashCell>
#endif
    AFK_WORLD_CACHE *worldCache;

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

    /* The landscape render queue.
     * These are transient objects -- delete them after
     * rendering.
     */
    AFK_RenderQueue<AFK_DisplayedLandscapeTile*> landscapeRenderQueue;

    /* The cell generating gang */
    AFK_AsyncGang<struct AFK_WorldCellGenParam, bool> *genGang;

    /* Cell generation worker delegates. */

    /* Generates this landscape tile, as necessary.
     * TODO: Something to put into a separate "landscape"
     * object if/when I make one.
     */
    bool generateClaimedLandscapeTile(
        const AFK_Tile& tile,
        AFK_LandscapeTile& landscapeTile,
        bool displayTerrain,
        unsigned int threadId,
        struct AFK_WorldCellGenParam param,
        ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue);

    /* Generates this world cell, as necessary. */
    bool generateClaimedWorldCell(
        AFK_WorldCell& worldCell,
        unsigned int threadId,
        struct AFK_WorldCellGenParam param,
        ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue);

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

    /* The number of rendered points each cell edge is
     * subdivided into.
     * With cubic cells, (pointSubdivisionFactor**2) is the
     * number of points per cell.
     */
    const unsigned int pointSubdivisionFactor;

    /* The size of the smallest cell. */
    const float minCellSize;


    AFK_World(
        float _maxDistance, 
        unsigned int _subdivisionFactor, 
        unsigned int _pointSubdivisionFactor,
        float _minCellSize,
        float startingDetailPitch);
    virtual ~AFK_World();

    /* Helper for the above -- requests a particular cell
     * at an offset (in cell scale multiples) from the
     * supplied one.
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const Vec3<long long>& modifier,
        const Vec3<float>& viewerLocation,
        const AFK_Camera& camera);

    /* Call when we're about to start a new frame. */
    void flipRenderQueues(void);

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
        ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue);
};

#endif /* _AFK_WORLD_H_ */

