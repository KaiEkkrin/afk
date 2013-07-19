/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <sstream>

#include "cell.hpp"
#include "core.hpp"
#include "exception.hpp"
#include "rng/rng.hpp"
#include "tile.hpp"


/* AFK_Tile implementation */

AFK_Tile::AFK_Tile()
{
    coord = afk_vec3<long long>(0, 0, 0);
}

bool AFK_Tile::operator==(const AFK_Tile& _tile) const
{
    return coord == _tile.coord;
}

bool AFK_Tile::operator!=(const AFK_Tile& _tile) const
{
    return coord != _tile.coord;
}

AFK_RNG_Value AFK_Tile::rngSeed() const
{
    return AFK_RNG_Value(coord.v[0], coord.v[1], coord.v[2]) ^ afk_core.config->masterSeed;
}

AFK_Tile AFK_Tile::parent(unsigned int subdivisionFactor) const
{
    long long parentTileScale = coord.v[2] * subdivisionFactor;
    return afk_tile(afk_vec3<long long>(
        ROUND_TO_CELL_SCALE(coord.v[0], parentTileScale),
        ROUND_TO_CELL_SCALE(coord.v[1], parentTileScale),
        parentTileScale));
}

Vec3<float> AFK_Tile::toWorldSpace(float worldScale) const
{
    return afk_vec3<float>(
        (float)coord.v[0] * worldScale / MIN_CELL_PITCH,
        (float)coord.v[1] * worldScale / MIN_CELL_PITCH,
        (float)coord.v[2] * worldScale / MIN_CELL_PITCH);
}

void AFK_Tile::enumerateHalfTiles(AFK_Tile *halfTiles, size_t halfTilesSize) const
{
    if (halfTilesSize != 4)
    {
        std::ostringstream ss;
        ss << "Tried to enumerate half tiles of count " << halfTilesSize;
        throw AFK_Exception(ss.str());
    }

    unsigned int halfTilesIdx = 0;
    for (long long xd = -1; xd <= 1; xd += 2)
    {
        for (long long zd = -1; zd <= 1; zd += 2)
        {
            halfTiles[halfTilesIdx].coord = afk_vec3<long long>(
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

    /* I choose a base tile.
     * TODO: What I'm hoping is that by choosing much larger base
     * tiles, I'll be able to have much larger (mutually
     * overlapping) terrain features.  Right now this isn't
     * the case: instead I just seem to be suffering from
     * overlap at a flat plane level.
     * What do I need to do to persuade it to build larger,
     * more overlapping bits of terrain?
     * P.S. I *think* that Mystery is just interlocking cones
     * (currently being cut off at the edges of larger cells)?
     */
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

AFK_Tile afk_tile(const Vec3<long long>& _coord)
{
    AFK_Tile tile;
    tile.coord = _coord;
    return tile;
}

AFK_Tile afk_tile(const AFK_Cell& cell)
{
    return afk_tile(afk_vec3<long long>(cell.coord.v[0], cell.coord.v[2], cell.coord.v[3]));
}

AFK_Cell afk_cell(const AFK_Tile& tile, long long yCoord)
{
    return afk_cell(afk_vec4<long long>(tile.coord.v[0], yCoord, tile.coord.v[1], tile.coord.v[2]));
}

size_t hash_value(const AFK_Tile &tile)
{
    return (
        tile.coord.v[0] ^
        LROTATE_UNSIGNED((unsigned long long)tile.coord.v[1], 17) ^
        LROTATE_UNSIGNED((unsigned long long)tile.coord.v[2], 37));
}

std::ostream& operator<<(std::ostream& os, const AFK_Tile& tile)
{
    return os << "Tile(" <<
        tile.coord.v[0] << ", " <<
        tile.coord.v[1] << ", scale " <<
        tile.coord.v[2] << ")";
}

