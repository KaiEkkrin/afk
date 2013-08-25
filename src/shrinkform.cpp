/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cstring>
#include <sstream>

#include "shrinkform.hpp"

std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformPoint& point)
{
    return os << "ShrinkformPoint(" <<
        "Location=(" << (int)point.s[AFK_SHO_POINT_X] << ", " << (int)point.s[AFK_SHO_POINT_Y] << ", " << (int)point.s[AFK_SHO_POINT_Z] << "), " <<
        "Weight=" << (int)point.s[AFK_SHO_POINT_WEIGHT] << ", Range=" << (int)point.s[AFK_SHO_POINT_RANGE];
}

/* AFK_ShrinkformCube implementation. */

Vec4<float> AFK_ShrinkformCube::getCubeCoord(void) const
{
    return coord;
}

void AFK_ShrinkformCube::make(
    std::vector<AFK_ShrinkformPoint>& points,
    const Vec4<float>& _coord,
    const AFK_ShapeSizes& sSizes,
    AFK_RNG& rng)
{
    coord = _coord;

    for (unsigned int i = 0; i < sSizes.pointCountPerCube; ++i)
    {
        AFK_ShrinkformPoint point;

        for (unsigned int j = 0; j < 8; ++j)
        {
            point.s[j] = (unsigned char)(rng.frand() * 256.0f);
        }

        points.push_back(point);
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformCube& cube)
{
    return os << "ShrinkformCube(Coord=" << cube.getCubeCoord() << ")";
}

/* AFK_ShrinkformList implementation. */

void AFK_ShrinkformList::extend(const std::vector<AFK_ShrinkformPoint>& points, const std::vector<AFK_ShrinkformCube>& cubes)
{
    unsigned int pOldSize = p.size();
    p.resize(p.size() + points.size());
    memcpy(&p[pOldSize], &points[0], points.size() * sizeof(AFK_ShrinkformPoint));

    unsigned int cOldSize = c.size();
    c.resize(c.size() + cubes.size());
    memcpy(&c[cOldSize], &cubes[0], cubes.size() * sizeof(AFK_ShrinkformCube));
}

void AFK_ShrinkformList::extend(const AFK_ShrinkformList& list)
{
    extend(list.p, list.c);
}

unsigned int AFK_ShrinkformList::pointCount(void) const
{
    return p.size();
}

unsigned int AFK_ShrinkformList::cubeCount(void) const
{
    return c.size();
}

