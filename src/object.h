/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_OBJECT_H_
#define _AFK_OBJECT_H_

#include "def.h"

/* General way of defining objects.  I'm not sure if I'm
 * going to want to keep this, but, you know.
 */

#define AXIS_PITCH  0
#define AXIS_YAW    1
#define AXIS_ROLL   2

class AFK_Object
{
public:

    /* Scale */
    Vec3<float> scale;

    /* An object can `pitch', `yaw', and `roll', which happen
     * around these three axes.
     * At start, the axes are pointing:
     * - pitch: to the right (+x)
     * - yaw: upwards (+y)
     * - roll: away from the camera (+z)
     * pitch is 0. yaw is 1. roll is 2.
     */
    Vec4<float> axes[3];

    /* The current accumulated rotation matrix. */
    Mat4<float> rotateMatrix;

    /* Location relative to the origin */
    Vec3<float> translate;
    
    AFK_Object();

    /* Adjusts the attitude of the object (pitch, yaw or roll),
     * which changes its rotation depending on what its current
     * attitude was. */
    virtual void adjustAttitude(int axis, float change);

    /* Get the object's various transformation matrices.
     * Combine in order: (camera) * translate * rotate * scale
     */
    virtual Mat4<float> getTranslation() const;
    virtual Mat4<float> getRotation() const;
    virtual Mat4<float> getScaling() const;
};

#endif /* _AFK_OBJECT_H_ */

