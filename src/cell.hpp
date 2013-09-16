/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CELL_H_
#define _AFK_CELL_H_

#include "afk.hpp"

#include <iostream>

#include <boost/static_assert.hpp>
#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_constructor.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

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


/* The C++ integer modulus operator's behaviour with
 * negative numbers is just shocking.
 * This utility is used for making the parents of both
 * cells and tiles
 */
#define AFK_ROUND_TO_CELL_SCALE(coord, scale) \
    (coord) - ((coord) >= 0 ? \
                ((coord) % (scale)) : \
                ((scale) + (((coord) % (scale)) != 0 ? \
                            ((coord) % (scale)) : \
                            -(scale))))


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

    /* TODO Somewhere, I'm going to need a way of making the
     * smallest AFK_Cell that can contain a particular object
     * at the current LoD, so that I can assign moving objects
     * to cells, decide what extra cells to draw or not etc etc.
     * That'll probably end up in AFK_World though ;)
     */

    /* Obligatory thingies. */
    bool operator==(const AFK_Cell& _cell) const;
    bool operator!=(const AFK_Cell& _cell) const;

    /* Gives the RNG seed value that matches this cell. */
    AFK_RNG_Value rngSeed() const;
    AFK_RNG_Value rngSeed(size_t combinant) const;

    /* A more general subdivide to use internally.
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
    /* TODO: I just tried converting this to a boost
     * iterator_facade and sadly, it looks like there's
     * some kind of fundamental thread safety issue with
     * it.
     * I got lots of holes in the landscape that should
     * not otherwise have been appearing.
     * Maybe at some point I should from-scratch my own
     * iterator-shaped thing for it.  :/
     */
    unsigned int subdivide(
        AFK_Cell *subCells,
        const size_t subCellsSize,
        unsigned int subdivisionFactor) const;

    /* Returns the parent cell to this one. */
    AFK_Cell parent(unsigned int subdivisionFactor) const;

    /* Checks whether the given cell could be a parent
     * to this one.  (Quicker than generating the
     * parent anew, doesn't require modulus)
     */
    bool isParent(const AFK_Cell& parent) const;

    /* Transforms this cell's co-ordinates to world space. */
    Vec4<float> toWorldSpace(float worldScale) const;
};

/* Useful ways of making cells. */
AFK_Cell afk_cell(const AFK_Cell& other);
AFK_Cell afk_cell(const Vec4<long long>& _coord);

/* Returns the cell (of the requested scale) that contains this point. */
AFK_Cell afk_cellContaining(const Vec3<float>& _coord, long long scale, float worldScale);

/* For insertion into an unordered_map. */
size_t hash_value(const AFK_Cell& cell);

/* This one is for the polymer cache. */
struct AFK_HashCell
{
    size_t operator()(const AFK_Cell& cell) const { return hash_value(cell); }
};

/* For printing an AFK_Cell. */
std::ostream& operator<<(std::ostream& os, const AFK_Cell& cell);

/* Important for being able to pass cells around in the queue. */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_Cell>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_constructor<AFK_Cell>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_Cell>::value));

#endif /* _AFK_CELL_H_ */

