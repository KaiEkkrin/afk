/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_3D_SOLID_H_
#define _AFK_3D_SOLID_H_

/* Describes the points that define a cube of
 * 3D solid.
 */

#include <sstream>
#include <vector>

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
    unsigned char f[8];

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DVapourFeature& feature);
};
std::ostream& operator<<(std::ostream& os, const AFK_3DVapourFeature& feature);

class AFK_3DVapourCube
{
protected:
    Vec4<float> coord;

    /* Worker functions for `make'. */
    void addRandomFeatureAtAdjacencyBit(
        std::vector<AFK_3DVapourFeature>& features,
        int adjBit,
        const Vec3<float>& coordMid,
        float slide,
        AFK_RNG& rng);

    void addRandomFeature(
        std::vector<AFK_3DVapourFeature>& features,
        int thisAdj,
        const Vec3<float>& coordMid,
        float slide,
        AFK_RNG& rng);

public:
    Vec4<float> getCubeCoord(void) const;

    void make(
        std::vector<AFK_3DVapourFeature>& features,
        const Vec4<float>& _coord,
        const AFK_Skeleton& skeleton,
        const AFK_ShapeSizes& sSizes,
        AFK_RNG& rng);

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
    void extend(const std::vector<AFK_3DVapourFeature>& features, const std::vector<AFK_3DVapourCube>& cubes);
    void extend(const AFK_3DList& list);

    unsigned int featureCount(void) const;
    unsigned int cubeCount(void) const;

    friend std::ostream& operator<<(std::ostream& os, const AFK_3DList& list);
};

std::ostream& operator<<(std::ostream& os, const AFK_3DList& list);

#endif /* _AFK_3D_SOLID_H_ */

