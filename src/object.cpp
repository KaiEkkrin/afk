/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include "object.h"

/* TODO I'm going to end up with a lot of these transformations (if I
 * have lots of objects whizzing about, all controlled through aircraft
 * like controls).  Candidate for processing in a batch with OpenCL?
 */

AFK_Object::AFK_Object()
{
    scale = Vec3<float>(1.0f, 1.0f, 1.0f);

    movement = Mat4<float>(
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   0.0f,
        0.0f,   0.0f,   0.0f,   1.0f
    );
}

/* I'm pretty sure that this transformation and the next are
 * correct for objects.
 */
void AFK_Object::adjustAttitude(enum AFK_Axes axis, float c)
{
    switch (axis)
    {
    case AXIS_PITCH:
        movement = movement * Mat4<float>(
            1.0f,   0.0f,       0.0f,       0.0f,
            0.0f,   cosf(c),    sinf(c),    0.0f,
            0.0f,   -sinf(c),   cosf(c),    0.0f,
            0.0f,   0.0f,       0.0f,       1.0f);
        break;

    case AXIS_YAW:
        movement = movement * Mat4<float>(
            cosf(c),    0.0f,   -sinf(c),   0.0f,
            0.0f,       1.0f,   0.0f,       0.0f,
            sinf(c),    0.0f,   cosf(c),    0.0f,
            0.0f,       0.0f,   0.0f,       1.0f);
        break;

    case AXIS_ROLL:
        movement = movement * Mat4<float>(
            cosf(c),    sinf(c),    0.0f,   0.0f,
            -sinf(c),   cosf(c),    0.0f,   0.0f,
            0.0f,       0.0f,       1.0f,   0.0f,
            0.0f,       0.0f,       0.0f,   1.0f);
        break;
    }
}

void AFK_Object::displace(enum AFK_Axes axis, float c)
{
    movement = movement * Mat4<float>(
        1.0f,   0.0f,   0.0f,   axis == AXIS_PITCH ? c: 0.0f,
        0.0f,   1.0f,   0.0f,   axis == AXIS_YAW ? c: 0.0f,
        0.0f,   0.0f,   1.0f,   axis == AXIS_ROLL ? c: 0.0f,
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
    adjustAttitude(AXIS_PITCH,  axisDisplacement.v[0]);
    adjustAttitude(AXIS_YAW,    axisDisplacement.v[1]);
    adjustAttitude(AXIS_ROLL,   axisDisplacement.v[2]);

    displace(AXIS_PITCH,        velocity.v[0]);
    displace(AXIS_YAW,          velocity.v[1]);
    displace(AXIS_ROLL,         velocity.v[2]);
}


Mat4<float> AFK_Object::getTransformation() const
{
    return movement * Mat4<float>(
        scale.v[0], 0.0f,       0.0f,       0.0f,
        0.0f,       scale.v[1], 0.0f,       0.0f,
        0.0f,       0.0f,       scale.v[2], 0.0f,
        0.0f,       0.0f,       0.0f,       1.0
    );
}

