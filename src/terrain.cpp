/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

#include "exception.hpp"
#include "terrain.hpp"

/* The methods for computing each individual
 * feature type.
 */
float AFK_TerrainFeature::compute_squarePyramid(const Vec3<float>& c) const
{
    float ty = 0.0f;

    if (c.v[0] < (location.v[0] - scale.v[0]) ||
        c.v[0] > (location.v[0] + scale.v[0]) ||
        c.v[2] < (location.v[2] - scale.v[2]) ||
        c.v[2] > (location.v[2] - scale.v[2]))
    {
        /* Outside the pyramid. */
        ty = c.v[1];
    }
    else
    {
        /* Make pyramid space co-ordinates. */
        float px = (c.v[0] - location.v[0]) / scale.v[0];
        float pz = (c.v[2] - location.v[2]) / scale.v[2];

        if (abs(px) > abs(pz))
        {
            /* We're on one of the left/right faces. */
            ty = c.v[1] + px * scale.v[1];
        }
        else
        {
            /* We're on one of the front/back faces. */
            ty = c.v[1] + pz * scale.v[1];
        }
    }

    return ty;
}

AFK_TerrainFeature::AFK_TerrainFeature(const AFK_TerrainFeature& f)
{
    type        = f.type;
    location    = f.location;
    scale       = f.scale;
}

AFK_TerrainFeature::AFK_TerrainFeature(
    enum AFK_TerrainType _type,
    const Vec3<float>& _location,
    const Vec3<float>& _scale)
{
    type        = _type;
    location    = _location;
    scale       = _scale;
}

AFK_TerrainFeature& AFK_TerrainFeature::operator=(const AFK_TerrainFeature& f)
{
    type        = f.type;
    location    = f.location;
    scale       = f.scale;
    return *this;
}

float AFK_TerrainFeature::compute(const Vec3<float>& c) const
{
    float ty = 0.0f;

    switch (type)
    {
    case AFK_TERRAIN_SQUARE_PYRAMID:
        ty = compute_squarePyramid(c);
        break;

    default:
        {
            std::ostringstream ss;
            ss << "Invalid terrain feature type: " << type;
            throw AFK_Exception(ss.str());
        }
    }

    return ty;
}

