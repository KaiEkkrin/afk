/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/functional/hash.hpp>
#include <cmath>
#include <sstream>

#include "cell.hpp"
#include "core.hpp"
#include "exception.hpp"
#include "rng/rng.hpp"
#include "terrain.hpp"


/* AFK_Cell implementation */

/* TODO Something is relying on this.  I don't know what.  It's highly
 * irritating.  But it doesn't matter for the purpose of being able to
 * add AFK_Cells to lockless queues.
 * (It's not AFK_RealCell .)
 */
AFK_Cell::AFK_Cell()
{
    coord = afk_vec4<long long>(0, 0, 0, 0);
}

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
    return AFK_RNG_Value(coord.v[0], coord.v[1], coord.v[2], coord.v[3]) ^ afk_core.config->masterSeed;
}

unsigned int AFK_Cell::subdivide(
    AFK_Cell *subCells,
    const size_t subCellsSize,
    long long stride,
    long long points) const
{
    /* Check whether we're at smallest subdivision */
    if (coord.v[3] == MIN_CELL_PITCH) return 0;

    /* Check for programming error */
    if (subCellsSize != (size_t)CUBE(points))
    {
        std::ostringstream ss;
        ss << "Supplied " << subCellsSize << " subcells for " << points << " points";
        throw AFK_Exception(ss.str());
    }

    AFK_Cell *subCellPos = subCells;
    unsigned int subCellCount = 0;
    for (long long i = coord.v[0]; i < coord.v[0] + stride * points; i += stride)
    {
        for (long long j = coord.v[1]; j < coord.v[1] + stride * points; j += stride)
        {
            for (long long k = coord.v[2]; k < coord.v[2] + stride * points; k += stride)
            {
                *(subCellPos++) = afk_cell(afk_vec4<long long>(i, j, k, stride));
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
        subdivisionFactor);
}

/* TODO I don't think I need this */
#if AUGMENTED_SUBCELLS
unsigned int AFK_Cell::augmentedSubdivide(AFK_Cell *augmentedSubcells, const size_t augmentedSubcellsSize) const
{
    return subdivide(
        augmentedSubcells,
        augmentedSubcellsSize,
        afk_core.world->subdivisionFactor,
        coord.v[3] / afk_core.world->subdivisionFactor,
        afk_core.world->subdivisionFactor + 1);
}
#endif

AFK_Cell AFK_Cell::parent(unsigned int subdivisionFactor) const
{
    long long parentCellScale = coord.v[3] * subdivisionFactor;
    return afk_cell(afk_vec4<long long>(
        ROUND_TO_CELL_SCALE(coord.v[0], parentCellScale),
        ROUND_TO_CELL_SCALE(coord.v[1], parentCellScale),
        ROUND_TO_CELL_SCALE(coord.v[2], parentCellScale),
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
        (float)coord.v[0] * worldScale / MIN_CELL_PITCH,
        (float)coord.v[1] * worldScale / MIN_CELL_PITCH,
        (float)coord.v[2] * worldScale / MIN_CELL_PITCH,
        (float)coord.v[3] * worldScale / MIN_CELL_PITCH);
}

AFK_Cell afk_cell(const AFK_Cell& other)
{
    AFK_Cell cell;
    cell.coord = other.coord;
    return cell;
}

AFK_Cell afk_cell(const Vec4<long long>& _coord)
{
    AFK_Cell cell;
    cell.coord = _coord;
    return cell;
}

size_t hash_value(const AFK_Cell& cell)
{
    /* Getting this thing right is quite important... */
    size_t hash = 0;
    boost::hash_combine(hash, cell.coord.v[0] * 0x0000c00180030006ll);
    boost::hash_combine(hash, cell.coord.v[1] * 0x00180030006000c0ll);
    boost::hash_combine(hash, cell.coord.v[2] * 0x00030006000c0018ll);
    boost::hash_combine(hash, cell.coord.v[3] * 0x006000c001800300ll);
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

