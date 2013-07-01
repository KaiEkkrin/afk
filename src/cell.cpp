/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

#include "cell.hpp"
#include "core.hpp"
#include "exception.hpp"
#include "terrain.hpp"


/* AFK_Cell implementation */

AFK_Cell::AFK_Cell()
{
    /* A cell size number of 0 indicates this AFK_Cell is invalid:
     * being able to express such an invalid value might be
     * useful.
     */
    coord = Vec4<long long>(0ll, 0ll, 0ll, 0ll);
}

AFK_Cell::AFK_Cell(const AFK_Cell& _cell)
{
    coord = _cell.coord;
}

AFK_Cell::AFK_Cell(const Vec4<long long>& _coord)
{
    coord = _coord;
}

AFK_Cell& AFK_Cell::operator=(const AFK_Cell& _cell)
{
    coord = _cell.coord;
    return *this;
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
    return AFK_RNG_Value(coord.v[0], coord.v[1], coord.v[2], coord.v[3]);
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
                *(subCellPos++) = AFK_Cell(Vec4<long long>(i, j, k, stride));
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
        afk_core.landscape->subdivisionFactor,
        coord.v[3] / afk_core.landscape->subdivisionFactor,
        afk_core.landscape->subdivisionFactor);
}

unsigned int AFK_Cell::augmentedSubdivide(AFK_Cell *augmentedSubcells, const size_t augmentedSubcellsSize) const
{
    return subdivide(
        augmentedSubcells,
        augmentedSubcellsSize,
        afk_core.landscape->subdivisionFactor,
        coord.v[3] / afk_core.landscape->subdivisionFactor,
        afk_core.landscape->subdivisionFactor + 1);
}

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
    long long parentCellScale = coord.v[3] * afk_core.landscape->subdivisionFactor;
    return AFK_Cell(Vec4<long long>(
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
        coord.v[2] < (parent.coord.v[2] + parent.coord.v[3]));
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


/* AFK_RealCell implementation */

void AFK_RealCell::enumerateHalfCells(AFK_RealCell *halfCells, size_t halfCellsSize) const
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
            halfCells[halfCellsIdx].worldCell = AFK_Cell(Vec4<long long>(
                worldCell.coord.v[0] + worldCell.coord.v[3] * xd / 2,
                worldCell.coord.v[1],
                worldCell.coord.v[2] + worldCell.coord.v[3] * zd / 2,
                worldCell.coord.v[3]));

            halfCells[halfCellsIdx].coord = Vec4<float>(
                coord.v[0] + coord.v[3] * xd / 2.0f,
                coord.v[1],
                coord.v[2] + coord.v[3] * zd / 2.0f,
                coord.v[3]);
        }
    }
}

AFK_RealCell::AFK_RealCell()
{
    worldCell       = AFK_Cell();
    coord           = Vec4<float>(0.0f, 0.0f, 0.0f, 1.0f);
}

AFK_RealCell::AFK_RealCell(const AFK_RealCell& realCell)
{
    worldCell       = realCell.worldCell;
    coord           = realCell.coord;
}

AFK_RealCell::AFK_RealCell(const AFK_Cell& cell, float minCellSize)
{
    worldCell       = cell;
    coord           = Vec4<float>(
        (float)cell.coord.v[0] * minCellSize,
        (float)cell.coord.v[1] * minCellSize,
        (float)cell.coord.v[2] * minCellSize,
        (float)cell.coord.v[3] * minCellSize);
}

AFK_RealCell& AFK_RealCell::operator=(const AFK_RealCell& realCell)
{
    worldCell       = realCell.worldCell;
    coord           = realCell.coord;
    return *this;
}

bool AFK_RealCell::testDetailPitch(unsigned int detailPitch, const AFK_Camera& camera, const Vec3<float>& viewerLocation) const
{
    /* Sample the centre of the cell.  This is wrong: it
     * will cause artifacts if you manage to get to exactly
     * the middle of a cell (I can probably test this with
     * the start position (8192, 8192, 8192)
     * TODO To fix it properly, I need to pick three points
     * displaced along the 3 axes by the dot pitch from the
     * centre of the cell, project them through the camera,
     * and compare those distances to the detail pitch,
     * no?
     * (in fact I'd probably get away with just the x and
     * z axes)
     */
    Vec3<float> centre = Vec3<float>(
        coord.v[0] + coord.v[3] / 2.0f,
        coord.v[1] + coord.v[3] / 2.0f,
        coord.v[2] + coord.v[3] / 2.0f);
    Vec3<float> facing = centre - viewerLocation;
    float distanceToViewer = facing.magnitude();

    /* Magic */
    float cellDetailPitch = camera.windowHeight * coord.v[3] /
        (camera.tanHalfFov * distanceToViewer);
    
    return cellDetailPitch < (float)detailPitch;
}

/* Helper for the below. */
static void testPointVisible(const Vec3<float>& point, const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible)
{
    /* Project the point.  This perspective projection
     * yields a viewport with the x co-ordinates
     * (-ar, ar) and the y co-ordinates (-1.0, 1.0).
     */
    Vec4<float> projectedPoint = camera.getProjection() * Vec4<float>(
        point.v[0], point.v[1], point.v[2], 1.0f);
    bool visible = (
        (projectedPoint.v[0] / projectedPoint.v[2]) >= -camera.ar &&
        (projectedPoint.v[0] / projectedPoint.v[2]) <= camera.ar &&
        (projectedPoint.v[1] / projectedPoint.v[2]) >= -1.0f &&
        (projectedPoint.v[1] / projectedPoint.v[2]) <= 1.0f);

    io_someVisible |= visible;
    io_allVisible &= visible;
}

void AFK_RealCell::testVisibility(const AFK_Camera& camera, bool& io_someVisible, bool& io_allVisible) const
{
    /* Check whether this cell is actually visible, by
     * testing all 8 vertices and its midpoint.
     * TODO Is that enough?
     */
    testPointVisible(Vec3<float>(
        coord.v[0],
        coord.v[1],
        coord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0] + coord.v[3],
        coord.v[1],
        coord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0],
        coord.v[1] + coord.v[3],
        coord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0] + coord.v[3],
        coord.v[1] + coord.v[3],
        coord.v[2]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0],
        coord.v[1],
        coord.v[2] + coord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0] + coord.v[3],
        coord.v[1],
        coord.v[2] + coord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0],
        coord.v[1] + coord.v[3],
        coord.v[2] + coord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0] + coord.v[3],
        coord.v[1] + coord.v[3],
        coord.v[2] + coord.v[3]),
        camera, io_someVisible, io_allVisible);

    testPointVisible(Vec3<float>(
        coord.v[0] + coord.v[3] / 2.0f,
        coord.v[1] + coord.v[3] / 2.0f,
        coord.v[2] + coord.v[3] / 2.0f),
        camera, io_someVisible, io_allVisible);
}

/* Define this in order to enable half-cell terrain.
 * (I do want to eventually do this.)
 */
#define HALFCELL_TERRAIN 0

void AFK_RealCell::makeTerrain(AFK_Terrain& terrain, AFK_RNG& rng) const
{
    /* Make the terrain cell for this actual cell. */
    AFK_TerrainCell terrainCell(coord);
    rng.seed(worldCell.rngSeed());
    terrainCell.make(rng);
    terrain.push(terrainCell);

    /* TODO Re-enable this when the basics look OK */
    /* Make the terrain cell for the four half-cells */
#if HALFCELL_TERRAIN
    AFK_RealCell halfCells[4];
    enumerateHalfCells(&halfCells[0], 4);
    for (unsigned int i = 0; i < 4; ++i)
    {
        AFK_TerrainCell terrainHalfCell(halfCells[i].coord);
        rng.seed(halfCells[i].worldCell.rngSeed());
        terrainHalfCell.make(rng);
        terrain.push(terrainHalfCell);
    }
#endif /* HALFCELL_TERRAIN */ 
}

void AFK_RealCell::removeTerrain(AFK_Terrain& terrain) const
{
#if HALFCELL_TERRAIN
    for (unsigned int i = 0; i < 5; ++i)
        terrain.pop();
#else
    terrain.pop();
#endif
}

