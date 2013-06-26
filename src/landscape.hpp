/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_LANDSCAPE_H_
#define _AFK_LANDSCAPE_H_

#include "afk.hpp"

#include <deque>
#include <iostream>

/* TODO: If I'm going to switch to C++11, I no longer need to
 * pull this from Boost
 */
#include <boost/unordered_map.hpp>

#include "camera.hpp"
#include "def.hpp"
#include "display.hpp"
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
 * TODO: Use of int here is not going to be sustainable if I
 * want an infinite world.  I am going to have to track the
 * position of the protagonist in infinite precision, and
 * conversion to an AFK_Cell will also want to input an infinite
 * precision number, otherwise the further you fly from the origin
 * the more aliased the world will get until it falls apart.
 * That means that I'm going to have to have a nasty thing by
 * which all the infinite-precision objects (there won't be that
 * many) track a local (single-precision float) co-ordinate too,
 * and every now and again I rebase the local co-ordinates and
 * run a task to update every single AFK_Object with the new
 * co-ordinates.
 * Blegh, hehe.
 * But in the first instance, I'll just use int, mapped from
 * AFK_Objects with single precision global co-ordinates, to
 * get something working.
 */
class AFK_Cell
{
public:
    /* These co-ordinates are in smallest-cell steps, starting
     * from (0,0,0) at the origin.  The 4th number is the
     * cell size: 1 for smallest, then increasing in factors
     * of subdivisionFactor.
     */
    Vec4<int> coord;

    AFK_Cell();
    AFK_Cell(const AFK_Cell& _cell);
    AFK_Cell(const Vec4<int>& _coord);

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

    /* Gives the cell's co-ordinates in world space, along
     * with its converted cell size.
     */
    Vec4<float> realCoord() const;

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

    /* Returns the parent cell to this one. */
    AFK_Cell parent(void) const;
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

    /* TODO Track the shader program and uniform variable
     * locations for all the landscape cells here?  (they'll
     * probably all be the same?)
     */

    /* The queue of landscape cells to display in the
     * next frame.
     */
    std::deque<AFK_LandscapeCell *> displayQueue;




    AFK_Landscape(size_t _cacheSize, float _maxDistance, unsigned int _subdivisionFactor, unsigned int _detailPitch);
    virtual ~AFK_Landscape();

    /* Enqueues a cell for drawing, creating it in the map
     * if it's not there already.
     */
    void enqueueCellForDrawing(const AFK_Cell& cell, const Vec4<float>& _coord);

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

