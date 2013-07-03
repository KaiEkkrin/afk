/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TERRAIN_H_
#define _AFK_TERRAIN_H_

/* Terrain-describing functions. */

/* TODO: Feature computation is an obvious candidate for
 * doing in a geometry shader, or in OpenCL.  Consider
 * this.  I'd need to build a long queue of features to
 * be computed, I guess.
 */

#include <vector>

#include "def.hpp"
#include "rng/rng.hpp"

/* The list of possible terrain features.
 * TODO: Include some better ones!
 */
enum AFK_TerrainType
{
    AFK_TERRAIN_FLAT                    = 0,
    AFK_TERRAIN_SQUARE_PYRAMID          = 1
};

/* TODO Keep this updated with the current feature
 * count.
 */
#define AFK_TERRAIN_FEATURE_TYPES 2

/* The encapsulation of any terrain feature.
 * A feature is computed at a particular location
 * based on the (x,y,z)
 * position that I want to compute it at, and
 * returns a displaced y co-ordinate for that position.
 * Terrain features are in cell co-ordinates (between
 * 0.0 and 1.0).
 *
 * I'm doing it like this so that I can pre-allocate
 * a single vector for all landscape terrain in the
 * landscape module, and don't end up thrashing the
 * heap making new objects for each feature.  This
 * restricts all features to the same parameters,
 * but I think that's okay.
 *
 * TODO:
 * Right now the feature is not allowed to displace
 * the x or z co-ordinates, only the y co-ordinate.
 * Clearly features for objects (??) would displace
 * all 3, but that's incompatible with the current
 * test for remaining within cell boundaries.
 * Do I want a different system for moving objects
 * (not cell restricted in the same way)?
 * Have a think about this.
 *
 * Can I cunningly finesse a remain-within-cell-
 * boundaries thing by means of "bouncing": compute
 * the co-ordinate modulo the cell boundary, and if
 * the co-ordinate divided by the cell boundary (to
 * integer) is odd, make the co-ordinate (1 - co-ordinate
 * modulo cell boundary), i.e. cause excess co-ordinates
 * to bounce back and forth between the cell walls?
 * Anyway, sort out terrain feature space and make
 * the basic thing render properly first.
 */
class AFK_TerrainFeature
{
protected:
    enum AFK_TerrainType    type;
    Vec3<float>             location;
    Vec3<float>             scale;

    /* The methods for computing each individual
     * terrain type.
     */
    void compute_flat(Vec3<float>& c) const;
    void compute_squarePyramid(Vec3<float>& c) const;

public:
    AFK_TerrainFeature() {}
    AFK_TerrainFeature(const AFK_TerrainFeature& f);
    AFK_TerrainFeature(
        const enum AFK_TerrainType _type,
        const Vec3<float>& _location,
        const Vec3<float>& _scale);

    AFK_TerrainFeature& operator=(const AFK_TerrainFeature& f);

    /* Computes in cell co-ordinates. and updates `c'.
     */
    void compute(Vec3<float>& c) const;
};

/* This describes a cell containing a collection of
 * terrain features.  It provides a method for computing
 * the total displacement applied by these features in
 * world co-ordinates.
 */

#define TERRAIN_FEATURE_COUNT_PER_CELL 1

class AFK_TerrainCell
{
protected:
    Vec4<float>         cellCoord; /* Like an AFK_RealCell */
    AFK_TerrainFeature  features[TERRAIN_FEATURE_COUNT_PER_CELL];
    unsigned int        featureCount;

    /* Tells compute() whether to clip the terrain at the
     * cell boundaries.  This should always be the case
     * unless deliberately trying to make holes in it
     * (e.g. with the starting cell -- cells not containing
     * the starting plane shouldn't have landscape.)
     */
    bool clip;

public:
    AFK_TerrainCell();
    AFK_TerrainCell(const AFK_TerrainCell& c);
    AFK_TerrainCell(const Vec4<float>& coord);

    AFK_TerrainCell& operator=(const AFK_TerrainCell& c);

    /* Call this to make a starting cell with a basic,
     * flat landscape.
     * `baseHeight' is a world-space height.
     */
    void start(float baseHeight);

    /* Assumes the RNG to have been seeded correctly for
     * the cell.
     */
    void make(unsigned int pointSubdivisionFactor, unsigned int subdivisionFactor, float minCellSize, AFK_RNG& rng);

    /* Computes in world co-ordinates each of the
     * terrain features and puts them together.
     * Updates `c' with the terrain modification if
     * there is terrain here (otherwise leaves it alone),
     * and returns true if there was terrain here, else
     * false.
     */
    bool compute(Vec3<float>& c) const;
};

/* This describes an entire terrain. */

class AFK_Terrain
{
protected:
    std::vector<AFK_TerrainCell> t;

public:
    AFK_Terrain() {}

    void push(const AFK_TerrainCell& cell);
    void pop();

    /* Like AFK_TerrainCell::compute(), but chains
     * together the entire terrain.
     */
    bool compute(Vec3<float>& c) const;
};


#endif /* _AFK_TERRAIN_H_ */

