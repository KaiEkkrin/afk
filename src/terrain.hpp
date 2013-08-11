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

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "landscape_sizes.hpp"
#include "rng/rng.hpp"

/* The list of possible terrain features.
 * TODO: Include some better ones!
 */
enum AFK_TerrainType
{
    AFK_TERRAIN_CONE                    = 2,
    AFK_TERRAIN_SPIKE                   = 3,
    AFK_TERRAIN_HUMP                    = 4
};

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

/* If you enable this, you also need to uncomment it in terrain.cl */
#define TILE_IN_FEATURE_DEBUG 0

class AFK_TerrainFeature
{
protected:
#if TILE_IN_FEATURE_DEBUG
    float               tileX;
    float               tileZ;
    float               tileScale;
    unsigned int        featureCount;
#endif

    Vec3<float>             tint; /* TODO Make location and tint features separate things? */
    Vec3<float>             scale;
    Vec2<float>             location; /* x-z */
    enum AFK_TerrainType    type;

public:
    AFK_TerrainFeature() {}
    AFK_TerrainFeature(
#if TILE_IN_FEATURE_DEBUG
        float _tileX,
        float _tileZ,
        float _tileScale,
        unsigned int _featureCount,
#endif
        const Vec3<float>& _tint,
        const Vec3<float>& _scale,
        const Vec2<float>& _location,
        const enum AFK_TerrainType _type);

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);
} __attribute__((aligned(16)));

std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);

/* This describes a tile with a collection of
 * terrain features.  It provides a method for computing
 * the total displacement applied by these features in
 * world co-ordinates.
 * Note that the structure, itself, does not in fact
 * contain the features, due to the difficulty of properly
 * packing them for OpenCL.  Instead you must keep the
 * feature list alongside the tile list, in the same
 * order.
 */

class AFK_TerrainTile
{
protected:
    /* Packing the tile co-ordinates like this lets me
     * halve the size of the structure when aligned for
     * OpenCL, which seems worthwhile, since I don't
     * directly compute on `tileCoord'
     */
    float               tileX;
    float               tileZ;
    float               tileScale;
    unsigned int        featureCount;

public:
    AFK_TerrainTile();

    Vec3<float> getTileCoord(void) const;

    /* Assumes the RNG to have been seeded correctly for
     * the tile.
     * If `forcedTint' is non-NULL, uses that tint for all
     * features generated here; otherwise, gets a tint off
     * of the RNG.
     */
    void make(
        std::vector<AFK_TerrainFeature>& features, /* We will append features to this */
        const Vec3<float>& _tileCoord,
        const AFK_LandscapeSizes &lSizes,
        unsigned int subdivisionFactor,
        float minCellSize,
        AFK_RNG& rng,
        const Vec3<float> *forcedTint);

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile);
};

std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_TerrainTile>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_TerrainTile>::value));

/* Encompasses the terrain as computed on a particular tile. */
class AFK_TerrainList
{
protected:
    std::vector<AFK_TerrainFeature> f;
    std::vector<AFK_TerrainTile> t;

public:
    /* Adds new TerrainTiles and TerrainFeatures to the list.
     * Make sure they're in order!  This function preserves the
     * mutual ordering.
     */
    void extend(const std::vector<AFK_TerrainFeature>& features, const std::vector<AFK_TerrainTile>& tiles);
    void extend(const AFK_TerrainList& list);

    unsigned int featureCount(void) const;
    unsigned int tileCount(void) const;
};

#endif /* _AFK_TERRAIN_H_ */

