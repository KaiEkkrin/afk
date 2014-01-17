/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

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
    Vec3<int64_t> coord;

    AFK_SkeletonCube();
    AFK_SkeletonCube(const Vec3<int64_t>& _coord);

    /* Makes a SkeletonCube out of a particular part of a
     * vapour cell.
     */
    AFK_SkeletonCube(const AFK_KeyedCell& vapourCell, const AFK_KeyedCell& shapeCell, const AFK_ShapeSizes& sSizes);

    /* Gives the adjacency for the given face (bottom, left, front, back, right, top). */
    AFK_SkeletonCube adjacentCube(int face) const;

    /* Gives the adjacency for the given offset,
     * where the offset components must be in the range -1 to 1 inclusive.
     */
    AFK_SkeletonCube adjacentCube(const Vec3<int64_t>& offset) const;

    /* Gives the upper cube for this cube. */
    AFK_SkeletonCube upperCube(const Vec3<int64_t>& upperOffset, unsigned int subdivisionFactor) const;

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
    uint64_t grid[afk_shapeSkeletonFlagGridDim][afk_shapeSkeletonFlagGridDim];
    int boneCount;

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
        const Vec3<int64_t>& upperOffset,
        AFK_RNG& rng,
        int subdivisionFactor,
        float bushiness);

public:
    AFK_Skeleton();

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
     */
    int make(
        const AFK_Skeleton& upper,
        const Vec3<int64_t>& upperOffset, /* I'll use the sub-cube of the upper grid from
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

    /* Like the above, but only flags a corner as adjacent if the
     * edges are, and only flags the edges if the faces are.
     * This is an adjacency without gaps, as it were.
     */
    int getCoAdjacency(const AFK_SkeletonCube& cube) const;

    /* This is a device for enumerating the cells that are
     * set within the skeleton.  They come out in skeleton
     * co-ordinates (between 0 and afk_shapeSkeletonFlagGridDim).
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

