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

#include "afk.hpp"

#include <sstream>

#include "cell.hpp"
#include "core.hpp"
#include "exception.hpp"
#include "rng/hash.hpp"
#include "rng/rng.hpp"
#include "tile.hpp"


/* AFK_Tile implementation */

AFK_Tile::AFK_Tile()
{
    coord = afk_vec3<int64_t>(0, 0, 0);
}

bool AFK_Tile::operator==(const AFK_Tile& _tile) const
{
    return coord == _tile.coord;
}

bool AFK_Tile::operator==(const AFK_Tile& _tile) const volatile
{
    return coord == _tile.coord;
}

bool AFK_Tile::operator!=(const AFK_Tile& _tile) const
{
    return coord != _tile.coord;
}

AFK_RNG_Value AFK_Tile::rngSeed() const
{
    size_t hash = hash_value(*this);
    AFK_RNG_Value seed;
    seed.v.ull[0] = (hash ^ afk_core.settings.masterSeedLow);
    seed.v.ull[1] = (hash ^ afk_core.settings.masterSeedHigh);
    return seed;
}

AFK_Tile AFK_Tile::parent(unsigned int subdivisionFactor) const
{
    int64_t parentTileScale = coord.v[2] * subdivisionFactor;
    return afk_tile(afk_vec3<int64_t>(
        AFK_ROUND_TO_CELL_SCALE(coord.v[0], parentTileScale),
        AFK_ROUND_TO_CELL_SCALE(coord.v[1], parentTileScale),
        parentTileScale));
}

Vec3<float> AFK_Tile::toWorldSpace(float worldScale) const
{
    return afk_vec3<float>(
        (float)coord.v[0] * worldScale,
        (float)coord.v[1] * worldScale,
        (float)coord.v[2] * worldScale);
}

void AFK_Tile::enumerateHalfTiles(AFK_Tile *halfTiles, size_t halfTilesSize) const
{
    if (halfTilesSize != 4)
    {
        std::ostringstream ss;
        ss << "Tried to enumerate half tiles of count " << halfTilesSize;
        throw AFK_Exception(ss.str());
    }

    if (coord.v[2] < 2) throw AFK_Exception("Tried to enumerate half tiles with overly small scale");

    unsigned int halfTilesIdx = 0;
    for (int64_t xd = -1; xd <= 1; xd += 2)
    {
        for (int64_t zd = -1; zd <= 1; zd += 2)
        {
            halfTiles[halfTilesIdx].coord = afk_vec3<int64_t>(
                coord.v[0] + coord.v[2] * xd / 2,
                coord.v[1] + coord.v[2] * zd / 2,
                coord.v[2]);

            ++halfTilesIdx;
        }
    }
}

void AFK_Tile::enumerateDescriptorTiles(AFK_Tile *tiles, size_t tilesSize, unsigned int subdivisionFactor) const
{
    if (tilesSize != 5)
    {
        std::ostringstream ss;
        ss << "Tried to enumerate descriptor tiles of count " << tilesSize;
        throw AFK_Exception(ss.str());
    }

    AFK_Tile baseTile = *this;
    tiles[0] = baseTile;

    /* The half tiles join up the terrain across base tiles
     * and make it seamless.
     * I hope.
     */
    baseTile.enumerateHalfTiles(&tiles[1], 4);
}

AFK_Tile afk_tile(const AFK_Tile& other)
{
    AFK_Tile tile;
    tile.coord = other.coord;
    return tile;
}

AFK_Tile afk_tile(const Vec3<int64_t>& _coord)
{
    AFK_Tile tile;
    tile.coord = _coord;
    return tile;
}

AFK_Tile afk_tile(const AFK_Cell& cell)
{
    return afk_tile(afk_vec3<int64_t>(cell.coord.v[0], cell.coord.v[2], cell.coord.v[3]));
}

AFK_Cell afk_cell(const AFK_Tile& tile, int64_t yCoord)
{
    return afk_cell(afk_vec4<int64_t>(tile.coord.v[0], yCoord, tile.coord.v[1], tile.coord.v[2]));
}

const AFK_Tile afk_unassignedTile = afk_tile(afk_vec3<int64_t>(0, 0, -1));

size_t hash_value(const AFK_Tile &tile)
{
    size_t hash = 0;
    hash = afk_hash_swizzle(hash, tile.coord.v[0]);
    hash = afk_hash_swizzle(hash, tile.coord.v[1]);
    hash = afk_hash_swizzle(hash, tile.coord.v[2]);
    return hash;
}

std::ostream& operator<<(std::ostream& os, const AFK_Tile& tile)
{
    return os << "Tile(" << std::dec <<
        tile.coord.v[0] << ", " <<
        tile.coord.v[1] << ", scale " <<
        tile.coord.v[2] << ", padding " <<
        tile.coord.v[3] << ")";
}

