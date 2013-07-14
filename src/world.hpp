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
#include "data/render_queue.hpp"
#include "def.hpp"
#include "displayed_world_cell.hpp"
#include "shader.hpp"
#include "world_cache.hpp"
#include "world_cell.hpp"

/* The world of AFK. */

class AFK_World;
class AFK_WorldCache;

/* The parameter for the cell generating worker.
 */
struct AFK_WorldCellGenParam
{
    AFK_Cell cell;
    AFK_World *world;
    Vec3<float> viewerLocation;
    const AFK_Camera *camera;
    unsigned int flags;
};

/* ...and this *is* the cell generating worker function */
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
    boost::atomic<unsigned long long> cellsEmpty;
    boost::atomic<unsigned long long> cellsInvisible;
    boost::atomic<unsigned long long> cellsQueued;
    boost::atomic<unsigned long long> cellsCached;
    boost::atomic<unsigned long long> cellsGenerated;

    /* The cache of world cells we're tracking.
     */
    AFK_WorldCache *cache;

    /* The render queue: cells to display next frame.
     * DOES NOT OWN THESE POINTERS.  DO NOT DELETE
     */
    AFK_RenderQueue<AFK_DisplayedWorldCell*> renderQueue;

    /* The cell generating gang */
    AFK_AsyncGang<struct AFK_WorldCellGenParam, bool> *genGang;

public:
    /* TODO Try to go around protecting these things.  Having it all
     * public is a bad plan.
     */

    /* World shader details. */
    AFK_ShaderProgram *shaderProgram;
    GLuint worldTransformLocation;
    GLuint clipTransformLocation;

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
    boost::unique_future<bool> updateLandMap(void);

    /* Draws the world in the current OpenGL context.
     * (There's no AFK_DisplayedObject for the world.)
     */
    void display(const Mat4<float>& projection);

    /* Takes a world checkpoint. */
    void checkpoint(boost::posix_time::time_duration& timeSinceLastCheckpoint);

    void printCacheStats(std::ostream& ss, const std::string& prefix);

    friend bool afk_generateWorldCells(
        unsigned int threadId,
        struct AFK_WorldCellGenParam param,
        ASYNC_QUEUE_TYPE(struct AFK_WorldCellGenParam)& queue);
};

#endif /* _AFK_WORLD_H_ */

