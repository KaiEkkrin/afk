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

    /* TODO AST I think I got this arbitrary axis doodah wrong;
     * I'm always applying a new rotation in object space, which
     * means the axes are the normal way round?
     * Anyway, keeping it for posterity.
     */
#ifdef ARBITRARY_AXIS_DOODAH
    /* An object can `pitch', `yaw', and `roll', which happen
     * around these three axes.
     * At start, the axes are pointing:
     * - pitch: to the right (+x)
     * - yaw: upwards (+y)
     * - roll: away from the camera (+z)
     * pitch is 0. yaw is 1. roll is 2.
     */
    Vec4<float> axes[3];
#endif

    /* The current accumulated rotation matrix. */
    Mat4<float> rotateMatrix;

    /* Translation */
    Vec3<float> translate;
    
    AFK_Object();

#ifdef ARBITRARY_AXIS_DOODAH
    /* Initialises the three axes based on the current rotation. */
    virtual void initAxes(void);
#endif

    /* Adjusts the attitude of the object (pitch, yaw or roll),
     * which changes its rotation depending on what its current
     * attitude was. */
    virtual void adjustAttitude(enum AFK_Axes axis, float change);

    /* Moves the object in space along a given axis. */
    virtual void displace(enum AFK_Axes axis, float change);

    /* Get the object's various transformation matrices.
     * Combine in order: (camera) * translate * rotate * scale
     */
    virtual Mat4<float> getTranslation() const;
    virtual Mat4<float> getRotation() const;
    virtual Mat4<float> getScaling() const;
};

#endif /* _AFK_OBJECT_H_ */

