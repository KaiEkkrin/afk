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

#include <boost/shared_ptr.hpp>

#include "def.hpp"
#include "rng/rng.hpp"

/* The list of possible terrain features.
 * TODO: Include some better ones!
 */
enum AFK_TerrainType
{
    AFK_TERRAIN_SQUARE_PYRAMID          = 0,
    AFK_TERRAIN_HUMP                    = 1
};

/* TODO Keep this updated with the current feature
 * count.
 */
#define AFK_TERRAIN_FEATURE_TYPES 1

/* The encapsulation of any terrain feature.
 * A feature is computed at a particular location
 * based on the (x,z)
 * position that I want to compute it at, and
 * returns a displaced y co-ordinate for that position.
 * Terrain features are in cell co-ordinates (between
 * 0.0 and 1.0).
 *
 * I'm doing it like this so that I can pre-allocate
 * a single vector for all landscape terrain in the
 * world module, and don't end up thrashing the
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
    Vec2<float>             location; /* x-z */
    Vec3<float>             tint; /* TODO Make location and tint features separate things? */
    Vec3<float>             scale;

    /* The methods for computing each individual
     * terrain type.
     */
    void compute_squarePyramid(Vec3<float>* positions, Vec3<float>* colours, size_t length) const;
    void compute_hump(Vec3<float>* positions, Vec3<float>* colours, size_t length) const;

public:
    AFK_TerrainFeature() {}
    AFK_TerrainFeature(const AFK_TerrainFeature& f);
    AFK_TerrainFeature(
        const enum AFK_TerrainType _type,
        const Vec2<float>& _location,
        const Vec3<float>& _tint,
        const Vec3<float>& _scale);

    AFK_TerrainFeature& operator=(const AFK_TerrainFeature& f);

    /* Computes in cell co-ordinates, updating the
     * y co-ordinate.
     */
    void compute(Vec3<float>* positions, Vec3<float>* colours, size_t length) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);

/* This describes a tile containing a collection of
 * terrain features.  It provides a method for computing
 * the total displacement applied by these features in
 * world co-ordinates.
 */

#define TERRAIN_FEATURE_COUNT_PER_TILE 4 
class AFK_TerrainTile
{
protected:
    Vec3<float>         tileCoord;
    AFK_TerrainFeature  features[TERRAIN_FEATURE_COUNT_PER_TILE];
    unsigned int        featureCount;

public:
    AFK_TerrainTile();
    AFK_TerrainTile(const AFK_TerrainTile& c);

    AFK_TerrainTile& operator=(const AFK_TerrainTile& c);

    const Vec3<float>& getTileCoord(void) const;
    Vec4<float> getCellCoord(void) const; /* y=0 */

    /* Assumes the RNG to have been seeded correctly for
     * the tile.
     * If `forcedTint' is non-NULL, uses that tint for all
     * features generated here; otherwise, gets a tint off
     * of the RNG.
     */
    void make(
        const Vec3<float>& _tileCoord,
        unsigned int pointSubdivisionFactor,
        unsigned int subdivisionFactor,
        float minCellSize,
        AFK_RNG& rng,
        const Vec3<float> *forcedTint);

    /* Transforms positions from this tile's co-ordinates to
     * the given one.
     */
    void transformTileToTile(Vec3<float> *positions, size_t length, const AFK_TerrainTile& other) const;

    /* Computes in cell co-ordinates each of the
     * terrain features and puts them together.
     */
    void compute(Vec3<float> *positions, Vec3<float> *colours, size_t length) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile);
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile);

/* Encompasses the terrain as computed on a particular tile. */
class AFK_TerrainList
{
protected:
    std::vector<boost::shared_ptr<AFK_TerrainTile> > t;

public:
    AFK_TerrainList();

    /* Adds a new TerrainTile to the list. */
    void add(boost::shared_ptr<AFK_TerrainTile> tile);

    /* Computes in world co-ordinates the terrain at the
     * given positions, transforming through the list as
     * it goes.
     */
    void compute(Vec3<float> *positions, Vec3<float> *colours, size_t length) const;
};

#endif /* _AFK_TERRAIN_H_ */

