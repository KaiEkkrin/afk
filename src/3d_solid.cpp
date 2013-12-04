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

#include <cassert>
#include <sstream>

#include "3d_solid.hpp"
#include "debug.hpp"

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourFeature& feature)
{
    return os << "3DVapourFeature(" <<
        "Location=(" << (int)feature.f[AFK_3DVF_X] << ", " << (int)feature.f[AFK_3DVF_Y] << ", " << (int)feature.f[AFK_3DVF_Z] << "), " <<
        "Colour=(" << (int)feature.f[AFK_3DVF_R] << ", " << (int)feature.f[AFK_3DVF_G] << ", " << (int)feature.f[AFK_3DVF_B] << "), " <<
        "Weight=" << (int)feature.f[AFK_3DVF_WEIGHT] << ", Range=" << (int)feature.f[AFK_3DVF_RANGE];
}

/* AFK_3DVapourCube implementation. */

static void getAxisMinMax(
    float mid,
    float slide,
    int adj, /* -1, 0, or 1 */
    float& o_min,
    float& o_max)
{
    assert(adj >= -1 && adj <= 1);
    
    switch (adj)
    {
    case -1:
        o_min = mid - slide;
        o_max = mid;
        break;

    case 0:
        o_min = mid;
        o_max = mid;
        break;

    case 1:
        o_min = mid;
        o_max = mid + slide;
        break;
    }
}

bool AFK_3DVapourCube::addRandomFeatureAtAdjacencyBit(
    AFK_3DVapourFeature& feature,
    int thisAdj,
    const Vec3<float>& coordMid,
    float slide,
    AFK_RNG& rng)
{
    /* Work out what this adjacency's deviation from the middle in
     * each of the 3 directions is.
     */
    Vec3<int> dev = afk_vec3<int>(0, 0, 0);
    bool foundDev = false;
    for (int x = -1; !foundDev && x <= 1; ++x)
    {
        for (int y = -1; !foundDev && y <= 1; ++y)
        {
            for (int z = -1; !foundDev && z <= 1; ++z)
            {
                if ((thisAdj & (1 << (9 * (x+1) + 3 * (y+1) + (z+1)))) != 0)
                {
                    dev = afk_vec3<int>(x, y, z);
                    foundDev = true;
                }
            }
        }
    }

    assert(foundDev);

    Vec3<float> coordMin, coordMax;
    for (int i = 0; i < 3; ++i)
    {
        getAxisMinMax(coordMid.v[i], slide, dev.v[i], coordMin.v[i], coordMax.v[i]);
    }

#define DEBUG_AXIS_MINMAX 0

#if DEBUG_AXIS_MINMAX
    std::ostringstream dbgs;
    dbgs << "Feature with axis min " << coordMin << ", axis max " << coordMax;
    int diffs = 0;
    for (int i = 0; i < 3; ++i)
    {
        assert(coordMax.v[i] >= coordMin.v[i]);
        if (coordMin.v[i] != coordMax.v[i]) ++diffs;
    }
    dbgs << " (" << diffs << " differences)";
    AFK_DEBUG_PRINTL(dbgs.str())
#endif

    /* Finally!  I've got the possible locations.  Fill out the feature. */
    int j;
    for (j = 0; j < 3; ++j)
    {
        feature.f[j] = (uint8_t)((rng.frand() * (coordMax.v[j] - coordMin.v[j]) + coordMin.v[j]) * 256.0f);
    }

    /* Non-location values can be arbitrary... */
    for (; j < 7; ++j)
    {
        feature.f[j] = (uint8_t)(rng.frand() * 256.0f);
    }

    /* ...except for the weight.
     * I want the weight to be:
     * - either between 0.5f and 1.0f (high chance)
     * - or between -1.0f and -0.5f (low chance).
     * After encoding, that means I want the weight to be
     * either between 0 and 127 (low chance) or 128 and 255
     * (high chance): the OpenCL will do the rest of the
     * transformation.
     */
    float weight = rng.frand() * 5.0f;
    if (weight >= 1.0f)
    {
        /* Get it between 0 and 1 ... */
        weight -= std::floor(weight);

        /* then expand it to 128-256 */
        weight = (weight * 128.0f) + 128.0f;
    }
    else
    {
        /* expand it to 0-128 */
        weight = weight * 128.0f;
    }

    feature.f[j] = (uint8_t)weight;
    return true;
}

bool AFK_3DVapourCube::addRandomFeature(
    AFK_3DVapourFeature& feature,
    int thisAdj,
    const Vec3<float>& coordMid,
    float slide,
    AFK_RNG& rng)
{
    /* Count up the number of ones in `thisAdj' */
    int thisAdjOnes = 0;
    for (unsigned int i = 0; i < (sizeof(int) * 8); ++i)
        if ((thisAdj & (1<<i)) != 0) ++thisAdjOnes;

    if (thisAdjOnes == 0) return false;

    /* Decide which adjacency to go for.
     * Note that sometimes I won't go for an adjacency but
     * instead drop out, allowing me to add a feature at a
     * lower adjacency instead.
     */
    unsigned int decider = (rng.uirand() & 0x7fffffff); /* no negatives, please */
    if ((decider & 3) == 0) return false;
    decider = decider >> 2;
    int decIndex = decider % thisAdjOnes;

    /* I'll draw a feature at a bit if it's unmasked and
     * the decider index points to it.
     */
    int decBit = 0;
    for (unsigned int i = 0; i < (sizeof(int) * 8); ++i)
    {
        if ((thisAdj & (1<<i)) != 0)
        {
            if (decIndex-- == 0)
            {
                decBit = (1<<i);
                break;
            }
        }
    }

    return addRandomFeatureAtAdjacencyBit(
        feature,
        decBit,
        coordMid,
        slide,
        rng);
}

Vec4<float> AFK_3DVapourCube::getCubeCoord(void) const
{
    return coord;
}

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourCube& cube)
{
    return os << "3DVapourCube(Coord=" << cube.getCubeCoord() << ")";
}

/* AFK_3DList implementation. */

void AFK_3DList::extend(const AFK_3DList& list)
{
    extend<std::vector<AFK_3DVapourFeature>, std::vector<AFK_3DVapourCube> >(list.f, list.c);
}

size_t AFK_3DList::featureCount(void) const
{
    return f.size();
}

size_t AFK_3DList::cubeCount(void) const
{
    return c.size();
}

/* Only enable this if you want a very great deal of spam */
#define PRINT_FEATURES 0

std::ostream& operator<<(std::ostream& os, const AFK_3DList& list)
{
    os << "3D List: Cubes: (";
    bool first = true;
    for (std::vector<AFK_3DVapourCube>::const_iterator cIt = list.c.begin();
        cIt != list.c.end(); ++cIt)
    {
        if (!first) os << ", ";
        os << *cIt;
        first = false;
    }

    os << ")";
#if PRINT_FEATURES
    os << "; Features: (";
    first = true;
    for (std::vector<AFK_3DVapourFeature>::const_iterator fIt = list.f.begin();
        fIt != list.f.end(); ++fIt)
    {
        if (!first) os << ", ";
        os << *fIt;
        first = false;
    }

    os << ")";
#endif
    return os;
}

