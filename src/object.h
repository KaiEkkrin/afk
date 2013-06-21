/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_OBJECT_H_
#define _AFK_OBJECT_H_

#include "def.h"

/* General way of defining objects.  I'm not sure if I'm
 * going to want to keep this, but, you know.
 */

enum AFK_Axes
{
    AXIS_PITCH  = 0,
    AXIS_YAW    = 1,
    AXIS_ROLL   = 2
};

class AFK_Object
{
public:

    /* Scale */
    Vec3<float> scale;

    /* The current accumulated movement (rotation and translation) */
    Mat4<float> movement;

    AFK_Object();

    /* Adjusts the attitude of the object (pitch, yaw or roll),
     * which changes its rotation depending on what its current
     * attitude was. */
    virtual void adjustAttitude(enum AFK_Axes axis, float c);

    /* Moves the object in space along a given axis.
     * TODO Can I change this into a velocity vector? */
    virtual void displace(enum AFK_Axes axis, float c);

    /* Drives the object, with a velocity and changes in the 3
     * axes of rotation. */
    virtual void drive(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement);

    /* Get the object's transformation matrix. */
    virtual Mat4<float> getTransformation() const;
};

#endif /* _AFK_OBJECT_H_ */

