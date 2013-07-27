/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_OBJECT_H_
#define _AFK_OBJECT_H_

#include <boost/type_traits/has_trivial_assign.hpp>
#include <boost/type_traits/has_trivial_destructor.hpp>

#include "def.hpp"

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
protected:
    /* The current accumulated transformation (scale, rotation and translation) */
    Mat4<float> transform;

public:
    AFK_Object();

    /* Scales the object.
     * Kinda only makes sense if you call it first ...
     */
    void scale(const Vec3<float>& s);

    /* Adjusts the attitude of the object (pitch, yaw or roll),
     * which changes its rotation depending on what its current
     * attitude was. */
    void adjustAttitude(enum AFK_Axes axis, float c);

    /* Moves the object in space */
    void displace(const Vec3<float>& v);

    /* Drives the object, with a velocity and changes in the 3
     * axes of rotation. */
    void drive(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement);

    /* Get the object's transformation matrix. */
    Mat4<float> getTransformation() const;
};

/* I want to be able to memcpy() these about, and so on. */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_Object>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_Object>::value));

#endif /* _AFK_OBJECT_H_ */

