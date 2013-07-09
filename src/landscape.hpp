/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_H_
#define _AFK_LANDSCAPE_H_

#include "afk.hpp"

#include <iostream>

#include <boost/atomic.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>

#include "async/async.hpp"
#include "camera.hpp"
#include "cell.hpp"
#include "data/cache.hpp"
#include "data/map_cache.hpp"
#include "data/polymer_cache.hpp"
#include "data/render_queue.hpp"
#include "def.hpp"
#include "display.hpp"
#include "frame.hpp"
#include "rng/rng.hpp"
#include "shader.hpp"
#include "terrain.hpp"

#if AFK_USE_POLYMER_CACHE
#define AFK_CACHE AFK_PolymerCache<AFK_Cell, AFK_LandscapeCell, boost::function<size_t (const AFK_Cell&)> >
#else
#define AFK_CACHE AFK_MapCache<AFK_Cell, AFK_LandscapeCell>
#endif

/* To start out with, I'm going to define an essentially
 * flat landscape split into squares for calculation and
 * rendering purposes.
 *
 *
 * Add improvements to make to it here.  There will be
 * lots !
 */


class AFK_LandscapeCell;

/* Describes the current state of one cell in the landscape.
 * TODO: Whilst AFK_DisplayedObject is the right abstraction,
 * I might be replicating too many fields here, making these
 * too big and clunky
 */
class AFK_DisplayedLandscapeCell: public AFK_DisplayedObject
{
public:
    /* This cell's triangle data. */
    struct AFK_VcolPhongVertex *triangleVs;
    unsigned int triangleVsCount;

    /* This will be a reference to the overall landscape
     * shader program.
     */
    GLuint program;

    /* The vertex set. */
    GLuint vertices;
    unsigned int vertexCount;

    AFK_DisplayedLandscapeCell(
        const AFK_LandscapeCell& landscapeCell,
        unsigned int pointSubdivisionFactor,
        AFK_CACHE& cache);
    virtual ~AFK_DisplayedLandscapeCell();

    virtual void initGL(void);
    virtual void displaySetup(const Mat4<float>& projection);
    virtual void display(const Mat4<float>& projection);
};

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedLandscapeCell& dlc);


#define TERRAIN_CELLS_PER_CELL 5

/* This is the value that we cache.
 */
class AFK_LandscapeCell
{
protected:
    /* This value says when the subcell enumerator last used
     * this cell.  If it's a long time ago, the cell will
     * become a candidate for eviction from the cache.
     */
    AFK_Frame lastSeen;

    /* The identity of the thread that last claimed use of
     * this cell.
     */
    boost::thread::id claimingThreadId;

    /* The obvious. */
    AFK_Cell cell;

    /* If this is a top level cell, don't go hunting in its parent
     * for stuff.
     */
    bool topLevel;

    /* The matching world co-ordinates.
     * TODO This is the thing that needs to change everywhere
     * when doing a rebase...  Of course, though since the
     * gen worker calls bind() on every cell it touches maybe
     * it will be easy...
     */
    Vec4<float> realCoord;

    /* The terrain at this cell.
     * There's one terrain cell that corresponds to this cell proper,
     * and four that correspond to a 1/2 cell offset in each diagonal
     * of the x-z plane.
     * Well-known RNG seeding keeps these consistent across cell seams.
     */
    bool hasTerrain;
    AFK_TerrainCell terrain[TERRAIN_CELLS_PER_CELL];

    /* Internal terrain computation. */
    void computeTerrainRec(Vec3<float>& position, Vec3<float>& colour, AFK_CACHE& cache) const;

public:
    /* The data for this cell's landscape, if we've
     * calculated it.
     */
    boost::shared_ptr<AFK_DisplayedLandscapeCell> displayed;

    AFK_LandscapeCell(): hasTerrain(false) {}
    AFK_LandscapeCell(const AFK_LandscapeCell& c);
    AFK_LandscapeCell& operator=(const AFK_LandscapeCell& c);

    const Vec4<float>& getRealCoord(void) const { return realCoord; }

    /* Tries to claim this landscape cell for processing by
     * the current thread.
     * I have no idea whether this is correct.  If something
     * kerpows, maybe it isn't...
     */
    bool claim(void);

    /* Binds a landscape cell to the world.  Needs to be called
     * before anything else gets done, because LandscapeCells
     * are created uninitialised in the cache.
     */
    void bind(const AFK_Cell& _cell, bool _topLevel, float worldScale);

    /* Tests whether this cell is within the specified detail pitch
     * when viewed from the specified location.
     */
    bool testDetailPitch(unsigned int detailPitch, const AFK_Camera& camera, const Vec3<float>& viewerLocation) const;

    /* Tests whether none, some or all of this cell's vertices are
     * visible when projected with the supplied camera.
     */
    void testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const;

    /* Adds terrain if there isn't any already. */
    void makeTerrain(
        unsigned int pointSubdivisionFactor,
        unsigned int subdivisionFactor,
        float minCellSize,
        const Vec3<float> *forcedTint);

    /* Computes the total terrain features here. */
    void computeTerrain(Vec3<float>& position, Vec3<float>& colour, AFK_CACHE& cache) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& landscapeCell);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& landscapeCell);


class AFK_Landscape;

/* The parameter for the cell generating worker. */
struct AFK_LandscapeCellGenParam
{
    AFK_Cell cell;
    bool topLevel;
    AFK_Landscape *landscape;
    Vec3<float> viewerLocation;
    const AFK_Camera *camera;
    bool entirelyVisible;   
};

/* ...and this *is* the cell generating worker function */
bool afk_generateLandscapeCells(
    struct AFK_LandscapeCellGenParam param,
    ASYNC_QUEUE_TYPE(struct AFK_LandscapeCellGenParam)& queue);


/* Encapsulates a whole landscape.  There will only be one of
 * these (added to AFK_Core).
 */
class AFK_Landscape
{
public:
    /* Landscape shader details. */
    AFK_ShaderProgram *shaderProgram;
    GLuint worldTransformLocation;
    GLuint clipTransformLocation;

    /* The cache of landscape cells we're tracking.
     */
    AFK_CACHE cache;

    /* The render queue: cells to display next frame.
     * DOES NOT OWN THESE POINTERS.  DO NOT DELETE
     */
    AFK_RenderQueue<AFK_DisplayedLandscapeCell*> renderQueue;

    /* The cell generating gang */
    AFK_AsyncGang<struct AFK_LandscapeCellGenParam, bool> genGang;

    /* Overall landscape parameters. */

    /* The distance to the furthest landscape cell to consider.
     * (You may want to use zFar, or maybe not.)
     */
    float maxDistance;

    /* The size of the biggest cell (across one dimension) shall
     * be equal to maxDistance; this is also the furthest-
     * away cell we'll consider drawing.  From there, cells get
     * subdivided by splitting each edge this number of times --
     * thus each cell gets split into this number cubed more
     * detailed cells
     */
    unsigned int subdivisionFactor;

    /* The maximum number of pixels each detail point on the
     * landscape should occupy on screen.  Determines when
     * to split a cell into smaller ones.
     * (I'm expecting to have to number-crunch a lot with this
     * value to get the right number to pass to the points
     * generator, but hopefully I'll only have to do that
     * once per landscape cell)
     * TODO: Can I use this and zNear to calculate the
     * maximum number of subdivisions?  I bet I can.
     */
    unsigned int detailPitch;

    /* The landscape starting height. */
    float baseHeight;

    /* The maximum number of subdivisions each cell can undergo.
     * An important result, because maximally subdivided cells
     * are separated by 1 in AFK_Cell coords. (see class
     * definition above.)
     */
    unsigned int maxSubdivisions;

    /* The number of rendered points each cell edge is
     * subdivided into.
     * With cubic cells, (pointSubdivisionFactor**2) is the
     * number of points per cell.
     */
    unsigned int pointSubdivisionFactor;

    /* The size of the smallest cell.  This is an important
     * result, because it's the amount I multiply the integer
     * cell co-ordinates by to get the float world
     * co-ordinates.
     */
    float minCellSize;

    /* Gather statistics.  (Useful.)
     */
    boost::atomic<unsigned int> cellsEmpty;
    boost::atomic<unsigned int> cellsInvisible;
    boost::atomic<unsigned int> cellsCached;
    boost::atomic<unsigned int> cellsQueued;


    AFK_Landscape(
        float _maxDistance, 
        unsigned int _subdivisionFactor, 
        unsigned int _detailPitch);
    virtual ~AFK_Landscape();

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

    /* This function drives the cell generating worker to
     * update the landscape cache and enqueue visible
     * cells.  Returns a future that becomes available
     * when we're done.
     */
    boost::unique_future<bool> updateLandMap(void);

    /* Draws the landscape in the current OpenGL context.
     * (There's no AFK_DisplayedObject for the landscape.)
     */
    void display(const Mat4<float>& projection);
};

#endif /* _AFK_LANDSCAPE_H_ */

