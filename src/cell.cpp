/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

#include "cell.hpp"
#include "core.hpp"
#include "exception.hpp"
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
    long long factor,
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

unsigned int AFK_Cell::subdivide(AFK_Cell *subCells, const size_t subCellsSize) const
{
    return subdivide(
        subCells,
        subCellsSize,
        afk_core.world->subdivisionFactor,
        coord.v[3] / afk_core.world->subdivisionFactor,
        afk_core.world->subdivisionFactor);
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

/* The C++ integer modulus operator's behaviour with
 * negative numbers is just shocking
 */
#define ROUND_TO_CELL_SCALE(coord, scale) \
    (coord) - ((coord) >= 0 ? \
                ((coord) % (scale)) : \
                ((scale) + (((coord) % (scale)) != 0 ? \
                            ((coord) % (scale)) : \
                            -(scale))))

AFK_Cell AFK_Cell::parent(void) const
{
    long long parentCellScale = coord.v[3] * afk_core.world->subdivisionFactor;
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

void AFK_Cell::enumerateHalfCells(AFK_Cell *halfCells, size_t halfCellsSize) const
{
    if (halfCellsSize != 4)
    {
        std::ostringstream ss;
        ss << "Tried to enumerate half cells of count " << halfCellsSize;
        throw AFK_Exception(ss.str());
    }

    unsigned int halfCellsIdx = 0;
    for (long long xd = -1; xd <= 1; xd += 2)
    {
        for (long long zd = -1; zd <= 1; zd += 2)
        {
            halfCells[halfCellsIdx].coord = afk_vec4<long long>(
                coord.v[0] + coord.v[3] * xd / 2,
                coord.v[1],
                coord.v[2] + coord.v[3] * zd / 2,
                coord.v[3]);

            ++halfCellsIdx;
        }
    }
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
    /* TODO This is only valid on 64-bit.
     * Hopefully declaring the local variables as type
     * `unsigned long long' below will result in a
     * compiler warning when I need reminding to fix
     * this.
     */
    
    unsigned long long xr, yr, zr, sr;

    xr = (unsigned long long)cell.coord.v[0];
    yr = (unsigned long long)cell.coord.v[1];
    zr = (unsigned long long)cell.coord.v[2];
    sr = (unsigned long long)cell.coord.v[3];

    asm("rol $13, %0\n" :"=r"(yr) :"0"(yr));
    asm("rol $29, %0\n" :"=r"(zr) :"0"(zr));
    asm("rol $43, %0\n" :"=r"(sr) :"0"(sr));

    return xr ^ yr ^ zr ^ sr;
}


/* The AFK_Cell print overload. */
std::ostream& operator<<(std::ostream& os, const AFK_Cell& cell)
{
    return os << "Cell(" <<
        cell.coord.v[0] << ", " <<
        cell.coord.v[1] << ", " <<
        cell.coord.v[2] << ", scale " <<
        cell.coord.v[3] << ")";
}

