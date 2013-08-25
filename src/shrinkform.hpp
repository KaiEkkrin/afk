/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_SHRINKFORM_H_
#define _AFK_SHRINKFORM_H_

/* Describes the points that define a cube of
 * shrinkform solid.
 */

#include <sstream>
#include <vector>

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"
#include "rng/rng.hpp"
#include "shape_sizes.hpp"

/* These need to stay in sync with shrinkform.cl which
 * does the actual computation.
 */
enum AFK_ShrinkformOffset
{
    AFK_SHO_POINT_X         = 0,
    AFK_SHO_POINT_Y         = 1,
    AFK_SHO_POINT_Z         = 2,
	AFK_SHO_POINT_R			= 3,
	AFK_SHO_POINT_G			= 4,
	AFK_SHO_POINT_B			= 5,
    AFK_SHO_POINT_WEIGHT    = 6,
    AFK_SHO_POINT_RANGE     = 7
};

class AFK_ShrinkformPoint
{
public:
    unsigned char s[8];

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformPoint& point);
};
std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformPoint& point);

class AFK_ShrinkformCube
{
protected:
    Vec4<float> coord;

public:
    Vec4<float> getCubeCoord(void) const;

    void make(
        std::vector<AFK_ShrinkformPoint>& points,
        const Vec4<float>& _coord,
        const AFK_ShapeSizes& sSizes,
        AFK_RNG& rng);

    friend std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformCube& cube);
};

std::ostream& operator<<(std::ostream& os, const AFK_ShrinkformCube& cube);

/* Important for being able to copy them around and
 * into the OpenCL buffers easily.
 */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_ShrinkformPoint>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_ShrinkformPoint>::value));

/* Encompasses all the shrinkform points that apply
 * to a particular compute pass.
 */
class AFK_ShrinkformList
{
protected:
    std::vector<AFK_ShrinkformPoint> p;
    std::vector<AFK_ShrinkformCube> c;

public:
    void extend(const std::vector<AFK_ShrinkformPoint>& points, const std::vector<AFK_ShrinkformCube>& cubes);
    void extend(const AFK_ShrinkformList& list);

    unsigned int pointCount(void) const;
    unsigned int cubeCount(void) const;
};

#endif /* _AFK_SHRINKFORM_H_ */

