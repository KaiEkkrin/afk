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
    const AFK_Skeleton& skeleton,
    const AFK_ShapeSizes& sSizes,
    AFK_RNG& rng)
{
    coord = _coord;

    /* I want to locate features around the skeleton, not away from it
     * (those would just get ignored).
     * To do this, enumerate the skeleton's bones...
     */
    std::vector<AFK_SkeletonCube> bones;
    AFK_Skeleton::Bones bonesEnum(skeleton);
    while (bonesEnum.hasNext()) bones.push_back(bonesEnum.next());

    for (unsigned int i = 0; i < sSizes.featureCountPerCube; ++i)
    {
        AFK_3DVapourFeature feature;

        unsigned int j;
        unsigned int b = rng.uirand() % bones.size();

        /* x, y and z must be near the bone. */
        /* TODO Experimentally twiddling this. */
#if 0
        for (j = 0; j < 3; ++j)
        {
            float coordMin = std::max(((((float)bones[b].coord.v[j]) - 0.5f) / (float)sSizes.skeletonFlagGridDim), 0.0f);
            float coordMax = std::min(((((float)bones[b].coord.v[j]) + 1.5f) / (float)sSizes.skeletonFlagGridDim), 1.0f);

            feature.f[j] = (unsigned char)((rng.frand() * (coordMax - coordMin) + coordMin) * 256.0f);
        }
#else
        for (j = 0; j < 3; ++j)
        {
            /* Let's try confining everything to the centre of the bone
             * to make sure I don't get side overlaps
             * (bet I do)
             */
            float coord = ((float)bones[b].coord.v[j] + 0.5f) / (float)sSizes.skeletonFlagGridDim;
            feature.f[j] = (unsigned char)(coord * 256.0f);
        }
#endif

        /* The rest can be arbitrary. */
        for (; j < 8; ++j)
        {
            if (j == 6)
            {
                /* TODO: Let's try making that radius quite small */
                feature.f[j] = (unsigned char)(
                    256.0f * 0.25f / (float)sSizes.skeletonFlagGridDim);
            }
            else
            {
                feature.f[j] = (unsigned char)(rng.frand() * 256.0f);
            }
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

