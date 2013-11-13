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

#include <cmath>
#include <sstream>

#include "cell.hpp"
#include "core.hpp"
#include "exception.hpp"
#include "rng/hash.hpp"
#include "rng/rng.hpp"
#include "terrain.hpp"


/* AFK_Cell implementation */

bool AFK_Cell::operator==(const AFK_Cell& _cell) const
{
    return coord == _cell.coord;
}

bool AFK_Cell::operator!=(const AFK_Cell& _cell) const
{
    return coord != _cell.coord;
}

AFK_RNG_Value AFK_Cell::rngSeed() const
{
    size_t hash = hash_value(*this);
    return AFK_RNG_Value(hash) ^ afk_core.config->masterSeed;
}

AFK_RNG_Value AFK_Cell::rngSeed(size_t combinant) const
{
    size_t hash = combinant;
    return AFK_RNG_Value(afk_hash_swizzle(hash, hash_value(*this), afk_factor5)) ^ afk_core.config->masterSeed;
}

unsigned int AFK_Cell::subdivide(
    AFK_Cell *subCells,
    const size_t subCellsSize,
    int64_t stride,
    int64_t points) const
{
    /* Check whether we're at smallest subdivision */
    if (coord.v[3] == 1) return 0;

    /* Check for programming error */
    if (subCellsSize != (size_t)CUBE(points))
    {
        std::ostringstream ss;
        ss << "Supplied " << subCellsSize << " subcells for " << points << " points";
        throw AFK_Exception(ss.str());
    }

    AFK_Cell *subCellPos = subCells;
    unsigned int subCellCount = 0;
    for (int64_t i = coord.v[0]; i < coord.v[0] + stride * points; i += stride)
    {
        for (int64_t j = coord.v[1]; j < coord.v[1] + stride * points; j += stride)
        {
            for (int64_t k = coord.v[2]; k < coord.v[2] + stride * points; k += stride)
            {
                *(subCellPos++) = afk_cell(afk_vec4<int64_t>(i, j, k, stride));
                ++subCellCount;
            }
        }
    }

    return subCellCount;
}

unsigned int AFK_Cell::subdivide(AFK_Cell *subCells, const size_t subCellsSize, unsigned int subdivisionFactor) const
{
    return subdivide(
        subCells,
        subCellsSize,
        coord.v[3] / subdivisionFactor,
        (int64_t)subdivisionFactor);
}

void AFK_Cell::faceAdjacency(AFK_Cell *adjacency, const size_t adjacencySize) const
{
    if (adjacencySize != 6) throw AFK_Exception("Bad face adjacency");

    adjacency[0] = afk_cell(afk_vec4<int64_t>(
        coord.v[0], coord.v[1] - coord.v[3], coord.v[2], coord.v[3]));
    adjacency[1] = afk_cell(afk_vec4<int64_t>(
        coord.v[0] - coord.v[3], coord.v[1], coord.v[2], coord.v[3]));
    adjacency[2] = afk_cell(afk_vec4<int64_t>(
        coord.v[0], coord.v[1], coord.v[2] - coord.v[3], coord.v[3]));
    adjacency[3] = afk_cell(afk_vec4<int64_t>(
        coord.v[0], coord.v[1], coord.v[2] + coord.v[3], coord.v[3]));
    adjacency[4] = afk_cell(afk_vec4<int64_t>(
        coord.v[0] + coord.v[3], coord.v[1], coord.v[2], coord.v[3]));
    adjacency[5] = afk_cell(afk_vec4<int64_t>(
        coord.v[0], coord.v[1] + coord.v[3], coord.v[2], coord.v[3]));
}

AFK_Cell AFK_Cell::parent(unsigned int subdivisionFactor) const
{
    int64_t parentCellScale = coord.v[3] * subdivisionFactor;
    return afk_cell(afk_vec4<int64_t>(
        AFK_ROUND_TO_CELL_SCALE(coord.v[0], parentCellScale),
        AFK_ROUND_TO_CELL_SCALE(coord.v[1], parentCellScale),
        AFK_ROUND_TO_CELL_SCALE(coord.v[2], parentCellScale),
        parentCellScale));
}

bool AFK_Cell::isParent(const AFK_Cell& parent) const
{
    /* The given cell could be parent if this cell falls
     * within its boundaries and the scale is correct
     */
    return (
        coord.v[0] >= parent.coord.v[0] &&
        coord.v[0] < (parent.coord.v[0] + parent.coord.v[3]) &&
        coord.v[1] >= parent.coord.v[1] &&
        coord.v[1] < (parent.coord.v[1] + parent.coord.v[3]) &&
        coord.v[2] >= parent.coord.v[2] &&
        coord.v[2] < (parent.coord.v[2] + parent.coord.v[3]) &&
        coord.v[3] < parent.coord.v[3]);
}

Vec4<float> AFK_Cell::toWorldSpace(float worldScale) const
{
    return afk_vec4<float>(
        (float)coord.v[0] * worldScale,
        (float)coord.v[1] * worldScale,
        (float)coord.v[2] * worldScale,
        (float)coord.v[3] * worldScale);
}

Vec4<float> AFK_Cell::toHomogeneous(float worldScale) const
{
    Vec4<float> worldSpace = toWorldSpace(worldScale);
    return afk_vec4<float>(worldSpace.v[0], worldSpace.v[1], worldSpace.v[2], 1.0f) / worldSpace.v[3];
}

AFK_Cell afk_cell(const AFK_Cell& other)
{
    AFK_Cell cell;
    cell.coord = other.coord;
    return cell;
}

AFK_Cell afk_cell(const Vec4<int64_t>& _coord)
{
    AFK_Cell cell;
    cell.coord = _coord;
    return cell;
}

const AFK_Cell afk_unassignedCell = afk_cell(afk_vec4<int64_t>(0, 0, 0, -1));

AFK_Cell afk_cellContaining(const Vec3<float>& _coord, int64_t scale, float worldScale)
{
    /* Turn these floats into something at minimum cell scale.
     * Note that float conversion to int64_t rounds towards
     * zero, which I don't want here, hence the std::floor()
     * first.
     */
    Vec3<int64_t> cellScaleCoord = afk_vec3<int64_t>(
        (int64_t)std::floor(_coord.v[0] / worldScale),
        (int64_t)std::floor(_coord.v[1] / worldScale),
        (int64_t)std::floor(_coord.v[2] / worldScale));

    /* ...and now, round them down to the next cell boundary
     * of the requested size
     */
    return afk_cell(afk_vec4<int64_t>(
        AFK_ROUND_TO_CELL_SCALE(cellScaleCoord.v[0], scale),
        AFK_ROUND_TO_CELL_SCALE(cellScaleCoord.v[1], scale),
        AFK_ROUND_TO_CELL_SCALE(cellScaleCoord.v[2], scale),
        scale));
}

size_t hash_value(const AFK_Cell& cell)
{
    /* Getting this thing right is quite important... */
    size_t hash = 0;
    hash = afk_hash_swizzle(hash, cell.coord.v[0], afk_factor1);
    hash = afk_hash_swizzle(hash, cell.coord.v[1], afk_factor2);
    hash = afk_hash_swizzle(hash, cell.coord.v[2], afk_factor3);
    hash = afk_hash_swizzle(hash, cell.coord.v[3], afk_factor4);
    return hash;
}


/* The AFK_Cell print overload. */
std::ostream& operator<<(std::ostream& os, const AFK_Cell& cell)
{
    return os << "Cell(" << std::dec <<
        cell.coord.v[0] << ", " <<
        cell.coord.v[1] << ", " <<
        cell.coord.v[2] << ", scale " <<
        cell.coord.v[3] << ")";
}

