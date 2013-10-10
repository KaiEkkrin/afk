/* AFK
 * Copyright (C) 2013, Alex Holloway.
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

#include "afk.hpp"

#include <cstring>
#include <sstream>

#include "computer.hpp"
#include "debug.hpp"
#include "exception.hpp"
#include "terrain.hpp"

std::ostream& operator<<(std::ostream& os, const AFK_TerrainFeature& feature)
{
    return os << "Feature(" <<
        "Scale=(" << (int)feature.f[AFK_TFO_SCALE_X] << ", " << (int)feature.f[AFK_TFO_SCALE_Y] << "), " <<
        "Location=(" << (int)feature.f[AFK_TFO_LOCATION_X] << ", " << (int)feature.f[AFK_TFO_LOCATION_Z] << ")";
}


/* AFK_TerrainTile implementation. */

float AFK_TerrainTile::getTileScale(void) const
{
    return tileScale;
}

Vec3<float> AFK_TerrainTile::getTileCoord(void) const
{
    return afk_vec3<float>(tileX, tileZ, tileScale);
}

void AFK_TerrainTile::make(
    std::vector<AFK_TerrainFeature>& features,
    const Vec3<float>& _tileCoord,
    const AFK_LandscapeSizes& lSizes,
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

        features.push_back(feature);
    }
} 

std::ostream& operator<<(std::ostream& os, const AFK_TerrainTile& tile)
{
    os << "TerrainTile(Coord=" << tile.getTileCoord() << ")";
    return os;
}


/* AFK_TerrainList implementation. */

void AFK_TerrainList::extend(const std::vector<AFK_TerrainFeature>& features, const std::vector<AFK_TerrainTile>& tiles)
{
    unsigned int fOldSize = f.size();
    f.resize(f.size() + features.size());
    memcpy(&f[fOldSize], &features[0], features.size() * sizeof(AFK_TerrainFeature));

    unsigned int tOldSize = t.size();
    t.resize(t.size() + tiles.size());
    memcpy(&t[tOldSize], &tiles[0], tiles.size() * sizeof(AFK_TerrainTile));
}

void AFK_TerrainList::extend(const AFK_TerrainList& list)
{
    extend(list.f, list.t);
}

unsigned int AFK_TerrainList::featureCount(void) const
{
    return f.size();
}

unsigned int AFK_TerrainList::tileCount(void) const
{
    return t.size();
}

