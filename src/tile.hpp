/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_TILE_H_
#define _AFK_TILE_H_

/* A Tile is like a Cell, but 2D (it lacks a y co-ordinate).  It's used
 * for splitting up the landscape, which is anchored at y=0.
 */

#include "afk.hpp"

#include "cell.hpp"

class AFK_Tile
{
public:
    AFK_Tile();

    Vec3<long long> coord;

    bool operator==(const AFK_Tile& _tile) const;
    bool operator!=(const AFK_Tile& _tile) const;

    AFK_RNG_Value rngSeed() const;

    /* Returns the parent tile to this one. */
    AFK_Tile parent(unsigned int subdivisionFactor) const;

    /* Transforms this tile's co-ordinates to world space.
     * (Still with an implied y co-ordinate of 0, of course.
     * The output vector is (x, z, scale). 
     */
    Vec3<float> toWorldSpace(float worldScale) const;

    /* TODO Turn this into an iterator -- saves on the array? */
    void enumerateHalfTiles(AFK_Tile *halfTiles, size_t halfTilesSize) const;

    /* Makes the 5 tiles that are used for the terrain at a spot. */
    void enumerateDescriptorTiles(AFK_Tile *tiles, size_t tilesSize, unsigned int subdivisionFactor) const;
};

AFK_Tile afk_tile(const AFK_Tile& other);
AFK_Tile afk_tile(const Vec3<long long>& _coord);
AFK_Tile afk_tile(const AFK_Cell& cell);

AFK_Cell afk_cell(const AFK_Tile& tile, long long yCoord);

/* For insertion into an unordered_map. */
size_t hash_value(const AFK_Tile& tile);

struct AFK_HashTile
{
    size_t operator()(const AFK_Tile& tile) const { return hash_value(tile); }
};

std::ostream& operator<<(std::ostream& os, const AFK_Tile& tile);

/* Important for being able to pass cells around in the queue. */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_Tile>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_Tile>::value));

#endif /* _AFK_TILE_H_ */

