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

#ifndef _AFK_3D_SOLID_H_
#define _AFK_3D_SOLID_H_

/* Describes the points that define a cube of
 * 3D solid.
 */

#include <array>
#include <sstream>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "rng/rng.hpp"
#include "shape_sizes.hpp"
#include "skeleton.hpp"

/* These need to stay in sync with shape_3d*.cl which
 * do the actual computation.
 */
enum AFK_3DVapourFeatureOffset
{
    AFK_3DVF_X         = 0,
    AFK_3DVF_Y         = 1,
    AFK_3DVF_Z         = 2,
    AFK_3DVF_R         = 3,
    AFK_3DVF_G         = 4,
    AFK_3DVF_B         = 5,
    AFK_3DVF_WEIGHT    = 6,
    AFK_3DVF_RANGE     = 7
};

class AFK_3DVapourFeature
{
public:
    uint8_t f[8];

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DVapourFeature& feature);
};
std::ostream& operator<<(std::ostream& os, const AFK_3DVapourFeature& feature);

class AFK_3DVapourCube
{
protected:
    Vec4<float> coord;

    /* Worker functions for `make'. */
    bool addRandomFeatureAtAdjacencyBit(
        AFK_3DVapourFeature& feature,
        int adjBit,
        const Vec3<float>& coordMid,
        float slide,
        AFK_RNG& rng);

    bool addRandomFeature(
        AFK_3DVapourFeature& feature,
        int thisAdj,
        const Vec3<float>& coordMid,
        float slide,
        AFK_RNG& rng);

public:
    Vec4<float> getCubeCoord(void) const;

    template<typename FeaturesIterator>
    void make(
        FeaturesIterator& featuresIt,
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
        std::vector<int> bonesCoAdjacency;
        int boneCount = skeleton.getBoneCount();
        
        bones.reserve(boneCount);
        bonesCoAdjacency.reserve(boneCount);
        
        AFK_Skeleton::Bones bonesEnum(skeleton);
        while (bonesEnum.hasNext())
        {
            AFK_SkeletonCube nextBone = bonesEnum.next();
            bones.push_back(nextBone);
            bonesCoAdjacency.push_back(skeleton.getCoAdjacency(nextBone));
        }
        
        int featuresAdded = 0;
        while (featuresAdded < sSizes.featureCountPerCube)
        {
            unsigned int selector = rng.uirand();
            unsigned int b = selector % bones.size();
            selector = selector / bones.size();
        
            /* This defines the centre of the bone that I picked. */
            Vec3<float> coordMid = afk_vec3<float>(
                ((float)bones[b].coord.v[0]) + 0.5f,
                ((float)bones[b].coord.v[1]) + 0.5f,
                ((float)bones[b].coord.v[2]) + 0.5f) / (float)sSizes.skeletonFlagGridDim;
        
            /* This defines the maximum displacement in any direction,
             * assuming suitable adjacency.
             */
            float slide = 0.5f / (float)sSizes.skeletonFlagGridDim;
        
            /* Go through each of the possible adjacencies to put a
             * feature near.  I'll prefer the earlier ones, because
             * they give me a wider range of movement, as it were.
             */
            const std::array<int, 4> tryFAdjArray = {
                0505000505,
                0252505252,
                0020252020,
                0000020000
            };

            for (int t : tryFAdjArray)
            {
                int thisAdj = (bonesCoAdjacency[b] & t);
                if (thisAdj != 0)
                {
                    if (addRandomFeature(
                        *featuresIt,
                        thisAdj,
                        coordMid,
                        slide,
                        rng))
                    {
                        ++featuresAdded;
                        ++featuresIt;
                        break;
                    }
                }
            }
        }
    }

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DVapourCube& cube);
};

std::ostream& operator<<(std::ostream& os, const AFK_3DVapourCube& cube);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_3DVapourCube>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_3DVapourCube>::value));

/* Encompasses all the cubes that apply
 * to a particular compute pass.
 */
class AFK_3DList
{
protected:
    std::vector<AFK_3DVapourFeature> f;
    std::vector<AFK_3DVapourCube> c;

public:
    template<typename FeaturesIterable, typename CubesIterable>
    void extend(const FeaturesIterable& features, const CubesIterable& cubes)
    {
        f.insert(f.end(), features.begin(), features.end());
        c.insert(c.end(), cubes.begin(), cubes.end());
    }

    void extend(const AFK_3DList& list);

    size_t featureCount(void) const;
    size_t cubeCount(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DList& list);
};

std::ostream& operator<<(std::ostream& os, const AFK_3DList& list);

#endif /* _AFK_3D_SOLID_H_ */

