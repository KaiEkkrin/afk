/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TERRAIN_H_
#define _AFK_TERRAIN_H_

/* Terrain-describing functions. */

/* TODO: Feature computation is an obvious candidate for
 * doing in a geometry shader, or in OpenCL.  Consider
 * this.  I'd need to build a long queue of features to
 * be computed, I guess.
 */

#include <deque>

#include "def.hpp"
#include "rng/rng.hpp"

/* The list of possible terrain features.
 * TODO: Include some better ones!
 */
enum AFK_TerrainType
{
    AFK_TERRAIN_SQUARE_PYRAMID          = 0
};

/* TODO Keep this updated with the current feature
 * count.
 */
#define AFK_TERRAIN_FEATURE_TYPES 1

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
 * I want a different system for objects.
 */
class AFK_TerrainFeature
{
protected:
    enum AFK_TerrainType    type;
    Vec3<float>             location; /* y is always 0 */
    Vec3<float>             tint;
    Vec3<float>             scale;

    /* The methods for computing each individual
     * terrain type.
     */
    void compute_squarePyramid(Vec3<float>& position, Vec3<float>& colour) const;

public:
    AFK_TerrainFeature() {}
    AFK_TerrainFeature(const AFK_TerrainFeature& f);
    AFK_TerrainFeature(
        const enum AFK_TerrainType _type,
        const Vec3<float>& _location,
        const Vec3<float>& _tint,
        const Vec3<float>& _scale);

    AFK_TerrainFeature& operator=(const AFK_TerrainFeature& f);

    /* Computes in cell co-ordinates, updating the
     * y co-ordinate.
     */
    void compute(Vec3<float>& position, Vec3<float>& colour) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);

/* This describes a cell containing a collection of
 * terrain features.  It provides a method for computing
 * the total displacement applied by these features in
 * world co-ordinates.
 */

#define TERRAIN_FEATURE_COUNT_PER_CELL 4 
class AFK_TerrainCell
{
protected:
    Vec4<float>         cellCoord; /* Like an AFK_RealCell, but y always 0 */
    AFK_TerrainFeature  features[TERRAIN_FEATURE_COUNT_PER_CELL];
    unsigned int        featureCount;

public:
    AFK_TerrainCell();
    AFK_TerrainCell(const AFK_TerrainCell& c);
    AFK_TerrainCell(const Vec4<float>& coord);

    AFK_TerrainCell& operator=(const AFK_TerrainCell& c);

    /* Assumes the RNG to have been seeded correctly for
     * the cell.
     * TODO Do something about the colour.
     */
    void make(const Vec3<float>& tint, unsigned int pointSubdivisionFactor, unsigned int subdivisionFactor, float minCellSize, AFK_RNG& rng);

    /* Computes in cell co-ordinates each of the
     * terrain features and puts them together.
     */
    void compute(Vec3<float>& position, Vec3<float>& colour) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainCell& cell);
    friend class AFK_Terrain;
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainCell& cell);

/* This describes an entire terrain. */

class AFK_Terrain
{
protected:
    /* TODO Change this back to a vector?  I suspect deque is
     * slow.  I can push_back() and pop_back() to the vector and
     * iterate through it in inverse order with the [] overload.
     */
    std::deque<AFK_TerrainCell> t;

public:
    AFK_Terrain() {}

    void push(const AFK_TerrainCell& cell);
    void pop();

    /* Like AFK_TerrainCell::compute(), but chains
     * together the entire terrain.  This time, the
     * input should be in world space (it will come
     * out like that, too).
     */
    void compute(Vec3<float>& position, Vec3<float>& colour) const;
};


#endif /* _AFK_TERRAIN_H_ */

