/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cstring>
#include <sstream>

#include "3d_solid.hpp"

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourFeature& feature)
{
    return os << "3DVapourFeature(" <<
        "Location=(" << (int)feature.f[AFK_3DVF_X] << ", " << (int)feature.f[AFK_3DVF_Y] << ", " << (int)feature.f[AFK_3DVF_Z] << "), " <<
        "Colour=(" << (int)feature.f[AFK_3DVF_R] << ", " << (int)feature.f[AFK_3DVF_G] << ", " << (int)feature.f[AFK_3DVF_B] << "), " <<
        "Weight=" << (int)feature.f[AFK_3DVF_WEIGHT] << ", Range=" << (int)feature.f[AFK_3DVF_RANGE];
}

/* AFK_3DVapourCube implementation. */

Vec4<float> AFK_3DVapourCube::getCubeCoord(void) const
{
    return coord;
}

void AFK_3DVapourCube::make(
    std::vector<AFK_3DVapourFeature>& features,
    const Vec4<float>& _coord,
    const AFK_ShapeSizes& sSizes,
    AFK_RNG& rng)
{
    coord = _coord;

    for (unsigned int i = 0; i < sSizes.featureCountPerCube; ++i)
    {
        AFK_3DVapourFeature feature;

        for (unsigned int j = 0; j < 8; ++j)
        {
            feature.f[j] = (unsigned char)(rng.frand() * 256.0f);
        }

        features.push_back(feature);
    }
}

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourCube& cube)
{
    return os << "3DVapourCube(Coord=" << cube.getCubeCoord() << ")";
}

/* AFK_3DList implementation. */

void AFK_3DList::extend(const std::vector<AFK_3DVapourFeature>& features, const std::vector<AFK_3DVapourCube>& cubes)
{
    unsigned int fOldSize = f.size();
    f.resize(f.size() + features.size());
    memcpy(&f[fOldSize], &features[0], features.size() * sizeof(AFK_3DVapourFeature));

    unsigned int cOldSize = c.size();
    c.resize(c.size() + cubes.size());
    memcpy(&c[cOldSize], &cubes[0], cubes.size() * sizeof(AFK_3DVapourCube));
}

void AFK_3DList::extend(const AFK_3DList& list)
{
    extend(list.f, list.c);
}

unsigned int AFK_3DList::featureCount(void) const
{
    return f.size();
}

unsigned int AFK_3DList::cubeCount(void) const
{
    return c.size();
}

