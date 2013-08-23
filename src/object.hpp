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
    /* These are the working values to update. */
    Vec3<float> scale;
    Vec3<float> translation;
    Quaternion<float> rotation;

public:
    AFK_Object();
    AFK_Object(const Vec3<float>& _scale, const Vec3<float>& _translation, const Quaternion<float>& _rotation);

    /* Scales the object. */
    void resize(const Vec3<float>& s);

    /* Rotates the object about an arbitrary axis. */
    void rotate(const Vec3<float>& axis, float c);

    /* Adjusts the attitude of the object (pitch, yaw or roll),
     * which changes its rotation depending on what its current
     * attitude was. */
    void adjustAttitude(enum AFK_Axes axis, float c);

    /* Moves the object in space */
    void displace(const Vec3<float>& v);

    /* Drives the object, with a velocity and changes in the 3
     * axes of rotation. */
    void drive(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement);

    /* Get the various component matrices of the transformation
     * matrix.
     */
    Mat4<float> getScaleMatrix() const;
    Mat4<float> getRotationMatrix() const;
    Mat4<float> getTranslationMatrix() const;

    /* Get the object's transformation matrix. */
    Mat4<float> getTransformation() const;
};

/* I want to be able to memcpy() these about, and so on. */
BOOST_STATIC_ASSERT((boost::has_trivial_assign<AFK_Object>::value));
BOOST_STATIC_ASSERT((boost::has_trivial_destructor<AFK_Object>::value));

#endif /* _AFK_OBJECT_H_ */

