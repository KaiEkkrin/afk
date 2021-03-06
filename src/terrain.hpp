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

enum AFK_TerrainFeatureOffset
{
    AFK_TFO_SCALE_X         = 0,
    AFK_TFO_SCALE_Y         = 1,
    AFK_TFO_LOCATION_X      = 2,
    AFK_TFO_LOCATION_Z      = 3,
    AFK_TFO_TINT_R          = 4,
    AFK_TFO_TINT_G          = 5,
    AFK_TFO_TINT_B          = 6,
    AFK_TFO_FTYPE           = 7
};

class AFK_TerrainFeature
{
public:
    /* Values stored in order as defined by the
     * AFK_TerrainFeatureOffset enum.
     */
    uint8_t f[8];

    friend std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature);
};

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
    float getTileScale(void) const;
    Vec3<float> getTileCoord(void) const;

    /* Assumes the RNG to have been seeded correctly for
     * the tile.  Treats `featuresIt' as an iterator, and
     * increments it as it writes features to it.
     * If `forcedTint' is non-NULL, uses that tint for all
     * features generated here; otherwise, gets a tint off
     * of the RNG.
     */
    template<typename FeaturesIterator>
    void make(
        FeaturesIterator& featuresIt,
        const Vec3<float>& _tileCoord,
        const AFK_LandscapeSizes &lSizes,
        AFK_RNG& rng)
    {
        /* This establishes where our terrain cell actually lies. */
        tileX = _tileCoord.v[0];
        tileZ = _tileCoord.v[1];
        tileScale = _tileCoord.v[2];
        
        /* For now I'm always going to apply `featureCountPerTile'
         * features instead, to avoid having padding issues in the
         * terrain compute queue.
         */
        for (unsigned int i = 0; i < lSizes.featureCountPerTile; ++i)
        {
            AFK_TerrainFeature feature;
        
            for (unsigned int j = 0; j < 7; ++j)
            {
                feature.f[j] = (uint8_t)(rng.frand() * 256.0f);
            }
        
            /* For now, I'm going to include one spike per tile,
             * and make the others humps.
             */
            feature.f[AFK_TFO_FTYPE] = (i == 0 ? AFK_TERRAIN_SPIKE : AFK_TERRAIN_HUMP);

            *(featuresIt++) = feature;
        }
    }

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

    template<typename FeaturesIterable, typename TilesIterable>
    void extend(const FeaturesIterable& features, const TilesIterable& tiles)
    {
        f.insert(f.end(), features.begin(), features.end());
        t.insert(t.end(), tiles.begin(), tiles.end());
    }

    void extend(const AFK_TerrainList& list);

    /* Extends the landscape by a single landscape tile's
     * features, from inplace.
     */
    void extendInplaceTiles(
        const volatile AFK_TerrainFeature *features,
        const volatile AFK_TerrainTile *tiles);

    size_t featureCount(void) const;
    size_t tileCount(void) const;
};

#endif /* _AFK_TERRAIN_H_ */

