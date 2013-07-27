/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <math.h>

#include "object.hpp"

/* TODO I'm going to end up with a lot of these transformations (if I
 * have lots of objects whizzing about, all controlled through aircraft
 * like controls).  Candidate for processing in a batch with OpenCL?
 */

AFK_Object::AFK_Object()
{
    transform = afk_mat4<float>(
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   0.0f,
        0.0f,   0.0f,   0.0f,   1.0f
    );
}

void AFK_Object::scale(const Vec3<float>& s)
{
    transform = transform * afk_mat4<float>(
        s.v[0],     0.0f,       0.0f,       0.0f,
        0.0f,       s.v[1],     0.0f,       0.0f,
        0.0f,       0.0f,       s.v[2],     0.0f,
        0.0f,       0.0f,       0.0f,       1.0f);
}

/* I'm pretty sure that this transformation and the next are
 * correct for objects.
 */
void AFK_Object::adjustAttitude(enum AFK_Axes axis, float c)
{
    switch (axis)
    {
    case AXIS_PITCH:
        transform = transform * afk_mat4<float>(
            1.0f,   0.0f,       0.0f,       0.0f,
            0.0f,   cosf(c),    sinf(c),    0.0f,
            0.0f,   -sinf(c),   cosf(c),    0.0f,
            0.0f,   0.0f,       0.0f,       1.0f);
        break;

    case AXIS_YAW:
        transform = transform * afk_mat4<float>(
            cosf(c),    0.0f,   -sinf(c),   0.0f,
            0.0f,       1.0f,   0.0f,       0.0f,
            sinf(c),    0.0f,   cosf(c),    0.0f,
            0.0f,       0.0f,   0.0f,       1.0f);
        break;

    case AXIS_ROLL:
        transform = transform * afk_mat4<float>(
            cosf(c),    sinf(c),    0.0f,   0.0f,
            -sinf(c),   cosf(c),    0.0f,   0.0f,
            0.0f,       0.0f,       1.0f,   0.0f,
            0.0f,       0.0f,       0.0f,   1.0f);
        break;
    }
}

void AFK_Object::displace(const Vec3<float>& v)
{
    transform = transform * afk_mat4<float>(
        1.0f,   0.0f,   0.0f,   v.v[0],
        0.0f,   1.0f,   0.0f,   v.v[1],
        0.0f,   0.0f,   1.0f,   v.v[2],
        0.0f,   0.0f,   0.0f,   1.0f);
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

    displace(velocity);
}


Mat4<float> AFK_Object::getTransformation() const
{
    return transform;
}

