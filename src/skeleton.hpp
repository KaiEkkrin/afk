/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SKELETON_H_
#define _AFK_SKELETON_H_

#include "afk.hpp"

#include "def.hpp"
#include "exception.hpp"
#include "keyed_cell.hpp"
#include "rng/rng.hpp"
#include "shape_sizes.hpp"

enum AFK_SkeletonFlag
{
    AFK_SKF_OUTSIDE_GRID,
    AFK_SKF_CLEAR,
    AFK_SKF_SET
};

/* This is the co-ordinate of a skeleton. */
class AFK_SkeletonCube
{
public:
    Vec3<long long> coord;

    AFK_SkeletonCube();
    AFK_SkeletonCube(const Vec3<long long>& _coord);

    /* Makes a SkeletonCube out of a particular part of a
     * vapour cell.
     */
    AFK_SkeletonCube(const AFK_KeyedCell& vapourCell, const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes);

    /* Gives the adjacency for the given face (bottom, left, front, back, right, top). */
    AFK_SkeletonCube adjacentCube(int face) const;

    /* Gives the upper cube for this cube. */
    AFK_SkeletonCube upperCube(const Vec3<long long>& upperOffset, unsigned int subdivisionFactor) const;

    /* Makes a KeyedCell describing the shape cell within a
     * vapour cell that would correspond to this skeleton cube.
     */
    AFK_KeyedCell toShapeCell(const AFK_KeyedCell& vapourCell, const AFK_ShapeSizes& sSizes) const;

    /* Using this cube as an iterator, advances it. */
    void advance(int gridDim);

    /* Using this cube as an iterator, tests for end. */
    bool atEnd(int gridDim) const;
};

/* This class describes a skeleton, as a grid of flags
 * saying whether the skeleton is present or not in each
 * cell of the grid.
 * It can drill down by `subdivisionFactor'.
 */

class AFK_Skeleton
{
protected:
    unsigned long long **grid;
    int gridDim;
    int boneCount;

    void initGrid(void);

    /* Simple queries. */
    enum AFK_SkeletonFlag testFlag(const AFK_SkeletonCube& where) const;
    void setFlag(const AFK_SkeletonCube& where);
    void clearFlag(const AFK_SkeletonCube& where);

    /* This "grows" a new skeleton by extending it from
     * this cube.
     * Returns the number of bones made.
     */
    int grow(
        const AFK_SkeletonCube& cube,
        int& bonesLeft,
        AFK_RNG& rng,
        const AFK_ShapeSizes& sSizes);

    /* This "embellishes" a coarser skeleton into a
     * finer one.
     * Returns the number of bones it made.
     */
    int embellish(
        const AFK_Skeleton& upper,
        const Vec3<long long>& upperOffset,
        AFK_RNG& rng,
        int subdivisionFactor,
        float bushiness);

public:
    AFK_Skeleton();
    virtual ~AFK_Skeleton();

    /* This makes a fresh Skeleton from scratch -- a
     * top level one.
     * Returns the number of bones made.  (If it didn't
     * make any, you could reasonably throw an
     * exception)
     */
    int make(
        AFK_RNG& rng,
        const AFK_ShapeSizes& sSizes);

    /* This makes a new Skeleton as a refinement of
     * the given one.  Returns the number of bones in the
     * skeleton.  This can end up as 0 (e.g. if there are
     * big gaps in the upper one), at which point you
     * can cancel enqueueing all the subsequent geometry!
     * TODO Do I instead want to feed this the entire
     * vapour cell cache?  I'm not sure I do, it would
     * be very messy ...
     */
    int make(
        const AFK_Skeleton& upper,
        const Vec3<long long>& upperOffset, /* I'll use the sub-cube of the upper grid from
                                             * upperOffset to (upperOffset + gridDim / sSizes.subdivisionFactor) in
                                             * each direction */
        AFK_RNG& rng,
        const AFK_ShapeSizes& sSizes);

    int getBoneCount(void) const;

    /* Checks whether a specific cube is within the skeleton. */
    bool within(const AFK_SkeletonCube& cube) const;

    /* Gets the adjacency for a particular skeleton cube, as
     * a bit field (bits 0-5 inclusive, usual order).
     */
    int getAdjacency(const AFK_SkeletonCube& cube) const;

    /* Gets the "full adjacency" for a particular skeleton cube:
     * flags between +1 and -1 inclusive in all 3 directions.
     * z is the units, y is the 3s, x is the 9s.
     */
    int getFullAdjacency(const AFK_SkeletonCube& cube) const;

    /* This is a device for enumerating the cells that are
     * set within the skeleton.  They come out in skeleton
     * co-ordinates (between 0 and gridDim).
     * It's not an iterator, because those are nasty and
     * I've had issues: I'm coming up with my own similar
     * paradigm instead (with less baggage).
     */
    class Bones
    {
    protected:
        const AFK_Skeleton& skeleton;
        AFK_SkeletonCube thisBone;
        AFK_SkeletonCube nextBone;

    public:
        Bones(const AFK_Skeleton& _skeleton);

        bool hasNext(void);
        AFK_SkeletonCube next(void);
    };
};

#endif /* _AFK_SKELETON_H_ */

