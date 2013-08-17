/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TERRAIN_H_
#define _AFK_TERRAIN_H_

/* Terrain-describing functions. */

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
 */

class AFK_TerrainFeature
{
protected:
    /* TODO: This needs compressing.  However, I'm not
     * sure right now what a sensible scheme would be.
     * Work on getting random shapes and entities working
     * next, and come back to it.
     */
    Vec3<float>             tint; /* TODO Make location and tint features separate things? */
    Vec3<float>             scale;
    Vec2<float>             location; /* x-z */
    enum AFK_TerrainType    type;

public:
    AFK_TerrainFeature() {}
    AFK_TerrainFeature(
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

public:
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
        AFK_RNG& rng);

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

