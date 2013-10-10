/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "core.hpp"
#include "entity.hpp"
#include "exception.hpp"


/* AFK_Entity implementation */

AFK_Entity::AFK_Entity(unsigned int _shapeKey):
    shapeKey(_shapeKey)
{
}

AFK_Entity::~AFK_Entity()
{
}

unsigned int AFK_Entity::getShapeKey(void) const
{
    return shapeKey;
}

void AFK_Entity::position(
    const Vec3<float>& scale,
    const Vec3<float>& displacement,
    const Vec3<float>& rotation)
{
    obj.resize(scale);
    obj.displace(displacement);
    if (rotation.v[0] != 0.0f) obj.adjustAttitude(AXIS_PITCH, rotation.v[0]);
    if (rotation.v[1] != 0.0f) obj.adjustAttitude(AXIS_YAW, rotation.v[1]);
    if (rotation.v[2] != 0.0f) obj.adjustAttitude(AXIS_ROLL, rotation.v[2]);
}

bool AFK_Entity::canBeEvicted(void) const
{
    /* We shouldn't actually be calling this.  I'm using
     * Claimable for the frame-tracking claim utility,
     * not as an evictable thing.
     */
    throw AFK_Exception("Wanted to evict an Entity");
}

