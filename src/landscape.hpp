/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_H_
#define _AFK_LANDSCAPE_H_

#include "afk.hpp"

#include <iostream>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread/thread.hpp>

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

/* To start out with, I'm going to define an essentially
 * flat landscape split into squares for calculation and
 * rendering purposes.
 *
 *
 * Add improvements to make to it here.  There will be
 * lots !
 */


/* Describes the current state of one cell in the landscape.
 * TODO: Whilst AFK_DisplayedObject is the right abstraction,
 * I might be replicating too many fields here, making these
 * too big and clunky
 */
class AFK_DisplayedLandscapeCell: public AFK_DisplayedObject
{
public:
    /* This will be a reference to the overall landscape
     * shader program.
     */
    GLuint program;

    /* The vertex set. */
    GLuint vertices;
    unsigned int vertexCount;

    AFK_DisplayedLandscapeCell(
        GLuint _program,
        GLuint _worldTransformLocation,
        GLuint _clipTransformLocation,
        const AFK_RealCell& cell,
        unsigned int pointSubdivisionFactor,
        const AFK_Terrain& terrain,
        AFK_RNG& rng);
    virtual ~AFK_DisplayedLandscapeCell();

    virtual void display(const Mat4<float>& projection);
};

std::ostream& operator<<(std::ostream& os, const AFK_DisplayedLandscapeCell& dlc);


/* This is the value that we cache.
 * It contains a link to the DisplayedLandscapeCell we've
 * rendered (if any).
 * TODO: This should also start to include a last-seen
 * timestamp, a link to dynamic objects in this cell, all
 * sorts of things.
 */
class AFK_LandscapeCell
{
public:
    /* This value says when the subcell enumerator last used
     * this cell.  If it's a long time ago, the cell will
     * become a candidate for eviction from the cache.
     */
    AFK_Frame lastSeen;

    /* The identity of the thread that last claimed use of
     * this cell.
     */
    boost::thread::id claimingThreadId;

    /* TODO: Things that might slot nicely into here:
     * - This cell's terrain cell
     */

    /* The data for this cell's landscape, if we've
     * calculated it.
     */
    boost::shared_ptr<AFK_DisplayedLandscapeCell> displayed;

    AFK_LandscapeCell();
    AFK_LandscapeCell(const AFK_LandscapeCell& c);
    AFK_LandscapeCell& operator=(const AFK_LandscapeCell& c);

    /* Tries to claim this landscape cell for processing by
     * the current thread.
     * I have no idea whether this is correct.  If something
     * kerpows, maybe it isn't...
     */
    bool claim(void);
};

std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& landscapeCell);



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
     * This is a global configured in afk.h
     */
#if AFK_USE_POLYMER_CACHE
    AFK_PolymerCache<AFK_Cell, AFK_LandscapeCell, boost::function<size_t (const AFK_Cell&)> > cache;
#else
    AFK_MapCache<AFK_Cell, AFK_LandscapeCell> cache;
#endif

    /* The render queue: cells to display next frame.
     * DOES NOT OWN THESE POINTERS.  DO NOT DELETE
     */
    AFK_RenderQueue<AFK_DisplayedLandscapeCell*> renderQueue;

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

    /* A working variable: the current terrain.  It's scoped
     * here to avoid cluttering the function parameter lists.
     * TODO: Parallelisation gotcha -- would need to share
     * the object with each thread as I went.  (Consider:
     * thread local storage; copying; etc.)
     */
    AFK_Terrain terrain;

    /* Gather statistics.  (Useful.)
     */
    unsigned int landscapeCellsEmpty;
    unsigned int landscapeCellsInvisible;
    unsigned int landscapeCellsCached;
    unsigned int landscapeCellsQueued;


    AFK_Landscape(
        float _maxDistance, 
        unsigned int _subdivisionFactor, 
        unsigned int _detailPitch);
    virtual ~AFK_Landscape();

    /* Enqueues this cell for drawing so long as its parent
     * is as supplied.  If it's less fine than the target
     * detail level, instead splits it recursively.
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const AFK_Cell& parent,
        const Vec3<float>& viewerLocation,
        const AFK_Camera& camera,
        bool entirelyVisible);

    /* The same, but with a vector modifier from the base
     * cell that should be multiplied up by the base cell's
     * scale.  Figures the parent out itself.
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const Vec3<long long>& modifier,
        const Vec3<float>& viewerLocation,
        const AFK_Camera& camera);

    /* Given the current position of the camera, this
     * function works out which cells to draw and fills
     * in the landMap appropriately.
     * TODO: I expect I'll want to split this up and assign
     * OpenCL work items to do the heavy lifting if things
     * start taking long! ;)
     */
    void updateLandMap(void);

    /* Draws the landscape in the current OpenGL context.
     * (There's no AFK_DisplayedObject for the landscape.)
     */
    void display(const Mat4<float>& projection);
};

#endif /* _AFK_LANDSCAPE_H_ */

