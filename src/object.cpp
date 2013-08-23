/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>
#include <sstream>

#include "exception.hpp"
#include "object.hpp"

/* TODO I'm going to end up with a lot of these transformations (if I
 * have lots of objects whizzing about, all controlled through aircraft
 * like controls).  Candidate for processing in a batch with OpenCL?
 */

AFK_Object::AFK_Object()
{
    scale = afk_vec3<float>(1.0f, 1.0f, 1.0f);
    translation = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    rotation = afk_quaternion<float>(0.0f, afk_vec3<float>(1.0f, 0.0f, 0.0f));
}

AFK_Object::AFK_Object(
    const Vec3<float>& _scale,
    const Vec3<float>& _translation,
    const Quaternion<float>& _rotation):
        scale(_scale), translation(_translation), rotation(_rotation)
{
}

void AFK_Object::resize(const Vec3<float>& s)
{
    scale = s;
}

void AFK_Object::rotate(const Vec3<float>& axis, float c)
{
    rotation = afk_quaternion(c, axis) * rotation;
}

/* I'm pretty sure that this transformation and the next are
 * correct for objects.
 */
void AFK_Object::adjustAttitude(enum AFK_Axes axis, float c)
{
    Vec3<float> axisVec;

    switch (axis)
    {
    case AXIS_PITCH:
        axisVec = rotation.rotate(afk_vec3<float>(1.0f, 0.0f, 0.0f));
        break;

    case AXIS_YAW:
        axisVec = rotation.rotate(afk_vec3<float>(0.0f, 1.0f, 0.0f));
        break;

    case AXIS_ROLL:
        axisVec = rotation.rotate(afk_vec3<float>(0.0f, 0.0f, 1.0f));
        break;

    default:
        std::ostringstream ss;
        ss << "Unrecognised axis of rotation: " << axis;
        throw AFK_Exception(ss.str());
    }

    rotate(axisVec, c);
}

void AFK_Object::displace(const Vec3<float>& v)
{
    translation += v;
}

void AFK_Object::drive(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement)
{
    /* In all cases I'm going to treat the axes individually.
     * This means theoretically doing lots more matrix multiplies,
     * but in practice if I tried to combine them I'd get a
     * headache and probably also have to do square roots, which
     * are no doubt more expensive.
     */
    if (axisDisplacement.v[0] != 0.0f)
        adjustAttitude(AXIS_PITCH,  axisDisplacement.v[0]);

    if (axisDisplacement.v[1] != 0.0f)
        adjustAttitude(AXIS_YAW,    axisDisplacement.v[1]);

    if (axisDisplacement.v[2] != 0.0f)
        adjustAttitude(AXIS_ROLL,   axisDisplacement.v[2]);

    /* I obtain the correct direction of displacement by rotating
     * the given vector by the object's current rotation quaternion
     */
    if (velocity.v[0] != 0.0f || velocity.v[1] != 0.0f || velocity.v[2] != 0.0f)
    {
        float speed;
        Vec3<float> direction;
        velocity.magnitudeAndDirection(speed, direction);
        direction = rotation.rotate(direction);
        displace(direction * speed);
    }
}

Mat4<float> AFK_Object::getScaleMatrix() const
{
    return afk_mat4<float>(
        scale.v[0], 0.0f,       0.0f,       0.0f,
        0.0f,       scale.v[1], 0.0f,       0.0f,
        0.0f,       0.0f,       scale.v[2], 0.0f,
        0.0f,       0.0f,       0.0f,       1.0f);
}

Mat4<float> AFK_Object::getRotationMatrix() const
{
    return rotation.rotationMatrix();
}

Mat4<float> AFK_Object::getTranslationMatrix() const
{
    return afk_mat4<float>(
        1.0f,       0.0f,       0.0f,       translation.v[0],
        0.0f,       1.0f,       0.0f,       translation.v[1],
        0.0f,       0.0f,       1.0f,       translation.v[2],
        0.0f,       0.0f,       0.0f,       1.0f);
}

Mat4<float> AFK_Object::getTransformation() const
{
    return getTranslationMatrix() * getRotationMatrix() * getScaleMatrix();
}

