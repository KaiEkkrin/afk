/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CELL_H_
#define _AFK_CELL_H_

#include "afk.hpp"

#include <iostream>

#include "camera.hpp"
#include "def.hpp"
#include "rng/rng.hpp"
#include "terrain.hpp"

/*
 * This module defines a cubic cell for
 * subdividing the world.  I may want to change this
 * later, but for now...  it'll do.
 */


#define MIN_CELL_PITCH 2



/* Identifies a cell in the world in an abstract manner,
 * suitable for using as a hash key.
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
     * cell size: 2 for smallest, then increasing in factors
     * of subdivisionFactor.
     * The reason for 2 being smallest is I need 1/2-cell steps
     * in order to fill in terrain that isn't interrupted at
     * every cell edge.
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


/* Describes a cell in terms of world co-ordinates.
 * Use for making the LandscapeCell that contains its
 * computed contents.
 */
class AFK_RealCell
{
protected:
    /* makeTerrain() helpers. */

    /* Terrain is composed of the terrain of the real cell plus that
     * of four make-believe cells displaced by 1/2 the distance from
     * the real cell to its neighbour siblings in each +/-
     * combination of the x and z axes.
     * This is done to avoid gutters along all the cells!
     */
    void enumerateHalfCells(AFK_RealCell *halfCells, size_t halfCellsSize) const;

public:
    AFK_Cell worldCell;
    Vec4<float> coord;

    AFK_RealCell();
    AFK_RealCell(const AFK_RealCell& realCell);
    AFK_RealCell(const AFK_Cell& cell, float minCellSize);

    AFK_RealCell& operator=(const AFK_RealCell& realCell);

    /* Tests whether this cell is within the specified detail pitch
     * when viewed from the specified location.
     */
    bool testDetailPitch(unsigned int detailPitch, const AFK_Camera& camera, const Vec3<float>& viewerLocation) const;

    /* Tests whether none, some or all of this cell's vertices are
     * visible when projected with the supplied camera.
     */
    void testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const;

    /* Makes this cell's terrain.  Pushes back the terrain
     * cells onto the terrain object.
     */
    void makeTerrain(AFK_Terrain& terrain, AFK_RNG& rng) const;

    /* Removes this cell's terrain from the terrain
     * object.  Doesn't work unless you called
     * makeTerrain() first.
     */
    void removeTerrain(AFK_Terrain& terrain) const;
};


#endif /* _AFK_CELL_H_ */

