/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_H_
#define _AFK_LANDSCAPE_H_

#include "afk.hpp"

#include <iostream>

/* TODO: If I'm going to switch to C++11, I no longer need to
 * pull this from Boost
 */
#include <boost/unordered_map.hpp>

#include "camera.hpp"
#include "def.hpp"
#include "display.hpp"
#include "rng/rng.hpp"
#include "shader.hpp"

/* To start out with, I'm going to define an essentially
 * flat landscape split into squares for calculation and
 * rendering purposes.
 *
 * This module is also defining a cubic cell for
 * subdividing the world.  I may want to change this
 * later, but for now...  it'll do.
 *
 * Add improvements to make to it here.  There will be
 * lots !
 */


/* Identifies a cell in the world.
 * TODO: Use of `long long' here means that, in theory at
 * least, the terrain will eventually loop.  I'm hoping that
 * it won't for a suitably long while and I won't need
 * infinite precision arithmetic (with all its associated
 * issues).
 * However, I *do* know that these don't fit into `float's
 * which will eventually become aliased, so at some point I'll
 * need to change the world render to eventually re-base its
 * view of the world to a place other than (0ll,0ll,0ll) as its
 * zero point.  Ngh.
 */
class AFK_Cell
{
public:
    /* These co-ordinates are in smallest-cell steps, starting
     * from (0,0,0) at the origin.  The 4th number is the
     * cell size: 1 for smallest, then increasing in factors
     * of subdivisionFactor.
     * TODO: These aren't homogeneous co-ordinates right now
     * (nor is the float `realCoord' equivalent) and for sanity,
     * I should probably make them homogeneous :P
     */
    Vec4<long long> coord;

    AFK_Cell();
    AFK_Cell(const AFK_Cell& _cell);
    AFK_Cell(const Vec4<long long>& _coord);

    /* TODO Somewhere, I'm going to need a way of making the
     * smallest AFK_Cell that can contain a particular object
     * at the current LoD, so that I can assign moving objects
     * to cells, decide what extra cells to draw or not etc etc.
     * That'll probably end up in AFK_Landscape though ;)
     */

    /* Obligatory thingies. */
    AFK_Cell& operator=(const AFK_Cell& _cell);
    bool operator==(const AFK_Cell& _cell) const;
    bool operator!=(const AFK_Cell& _cell) const;

    /* Gives the RNG seed value that matches this cell. */
    AFK_RNG_Value rngSeed() const;

    /* Gives the cell's co-ordinates in world space, along
     * with its converted cell size.
     */
    Vec4<float> realCoord() const;

    /* A more general subdivide to use internally.
     * `factor' is the number to divide the scale by.
     * `stride' is the gap to put between each subcell
     * and the next.
     * `points' is the number of subcells to put along
     * each dimension (which may be different from `factor'
     * if I want to run off the edge of the parent, see
     * augmentedSubdivide() ! )
     */
    unsigned int subdivide(
        AFK_Cell *subCells,
        const size_t subCellsSize,
        long long factor,
        long long stride,
        long long points) const;

    /* Fills out the supplied array of uninitialised
     * AFK_Cells with the next level of subdivision of
     * the current cell.
     * This function will want to fill out
     * (subdivisionFactor^3) cells.
     * subCellsSize is in units of sizeof(AFK_Cell).
     * Returns 0 if we're at the smallest subdivision
     * already, else the number of subcells made.
     */
    unsigned int subdivide(AFK_Cell *subCells, const size_t subCellsSize) const;

    /* This does the same, except it also includes the
     * nearest subcells of the adjacent cells + along
     * each axis.  The idea is that each cell is described
     * by its lowest vertex (along each axis) and like this,
     * I'll enumerate all subcell vertices.
     * This function will want to fill out
     * ((subdivisionFactor+1)^3) cells.
     * Returns 0 if we're at the smallest subdivision
     * already, else the number of subcells made.
     */
    unsigned int augmentedSubdivide(AFK_Cell *augmentedSubcells, const size_t augmentedSubcellsSize) const;

    /* Returns the parent cell to this one. */
    AFK_Cell parent(void) const;

    /* Checks whether the given cell could be a parent
     * to this one.  (Quicker than generating the
     * parent anew, doesn't require modulus)
     */
    bool isParent(const AFK_Cell& parent) const;
};

/* For insertion into an unordered_map. */
size_t hash_value(const AFK_Cell& cell);

/* For printing an AFK_Cell. */
std::ostream& operator<<(std::ostream& os, const AFK_Cell& cell);

/* Describes the current state of one cell in the landscape.
 * TODO: Whilst AFK_DisplayedObject is the right abstraction,
 * I might be replicating too many fields here, making these
 * too big and clunky
 */
class AFK_LandscapeCell: public AFK_DisplayedObject
{
public:
    /* The cell location and scale in world space. */
    Vec4<float> coord;

    /* The vertex set. */
    GLuint vertices;

    AFK_LandscapeCell(const AFK_Cell& cell, const Vec4<float>& _coord); /* from AFK_Cell::realCoord() */
    virtual ~AFK_LandscapeCell();

    virtual void display(const Mat4<float>& projection);
};

/* For printing an AFK_LandscapeCell. */
std::ostream& operator<<(std::ostream& os, const AFK_LandscapeCell& cell);


/* Encapsulates a whole landscape.  There will only be one of
 * these (added to AFK_Core).
 */
class AFK_Landscape
{
public:
    /* Landscape shader details. */
    AFK_ShaderProgram *shaderProgram;
    GLuint transformLocation;
    GLuint fixedColorLocation;

    /* Maps a cell identifier to its displayed landscape
     * object.  For now, the displayed landscape object
     * actually contains all the data.  This may or may
     * not be a good idea.
     * TODO: I don't think I want to keep this structure.  I
     * can't pick an element randomly from it, which means
     * I'm going to have horrible cache eviction artifacts.
     * Instead, after I've gotten the basic thing working
     * with this, I should write my own structure that
     * provides the proper interface I want.
     */
    boost::unordered_map<const AFK_Cell, AFK_LandscapeCell*> landMap;

    /* Overall landscape parameters. */

    /* The cache size to use.
     * I'm going to allocate the whole damn thing right away,
     * you know.
     */
    size_t cacheSize;

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
    unsigned int landscapeCellsEmpty;
    unsigned int landscapeCellsInvisible;
    unsigned int landscapeCellsCached;
    unsigned int landscapeCellsQueued;

    /* The queue of landscape cells to display in the
     * next frame.
     * This is actually implemented as a map just like the
     * cache.  It doesn't matter what order they get picked
     * in, but it does matter that I can quickly avoid
     * re-adding a cell if it's in there already.
     * The bool is used to flag a cell that's been analysed
     * and subdivided; it means "no LandscapeCell to render
     * here, but don't try to analyse it again"
     */
    boost::unordered_map<const AFK_Cell, std::pair<bool, AFK_LandscapeCell*> > landQueue;


    AFK_Landscape(size_t _cacheSize, float _maxDistance, unsigned int _subdivisionFactor, unsigned int _detailPitch);
    virtual ~AFK_Landscape();

    /* Utility function.  Calculates the apparent cell detail
     * pitch at a cell vertex.
     */
    float getCellDetailPitch(const AFK_Cell& cell, const Vec3<float>& viewerLocation) const;

    /* Utility function.  Tests whether a cell is within the
     * intended detail pitch.
     */
    bool testCellDetailPitch(const AFK_Cell& cell, const Vec3<float>& viewerLocation) const;

    /* Utility function.  Tests whether a point is visible.
     * Cumulatively ANDs visibility to `allVisible', and ORs
     * visibility to `someVisible'.
     */
    void testPointVisible(
        const Vec3<float>& point,
        const Mat4<float>& projection,
        bool& io_someVisible,
        bool& io_allVisible) const;

    /* Utility function.  Tests whether there's anything to
     * draw in a cell.
     */
    bool testCellEmpty(const AFK_Cell& cell) const;

    /* Enqueues this cell for drawing so long as its parent
     * is as supplied.  If it's less fine than the target
     * detail level, instead splits it recursively.
     * TODO: In order to make a correct call to testCellEmpty(),
     * I'm going to want to cumulate the single-vertex landscape
     * sample here (which testCellEmpty() will want).
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const AFK_Cell& parent,
        const Vec3<float>& viewerLocation,
        const Mat4<float>& projection,
        bool entirelyVisible);

    /* The same, but with a vector modifier from the base
     * cell that should be multiplied up by the base cell's
     * scale.  Figures the parent out itself.
     */
    void enqueueSubcells(
        const AFK_Cell& cell,
        const Vec3<long long>& modifier,
        const Vec3<float>& viewerLocation,
        const Mat4<float>& projection);

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

