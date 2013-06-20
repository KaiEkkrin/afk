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

    rotateMatrix = Mat4<float>(
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   0.0f,
        0.0f,   0.0f,   0.0f,   1.0f
    );

    translate = Vec3<float>(1.0f, 1.0f, 1.0f);

#ifdef ARBITRARY_AXIS_DOODAH
    initAxes();
#endif
}

#ifdef ARBITRARY_AXIS_DOODAH
void AFK_Object::initAxes(void)
{
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
            axes[i].v[j] = (i == j ? 1.0f : 0.0f);
        axes[i].v[3] = 1.0f;
        axes[i] = rotateMatrix * axes[i];
    }
}
#endif

void AFK_Object::adjustAttitude(enum AFK_Axes axis, float c)
{
#ifdef ARBITRARY_AXIS_DOODAH
    float x, y, z;

    x = axes[axis].v[0];
    y = axes[axis].v[1];
    z = axes[axis].v[2];

    /* This arbitrary-axis rotation formula from "Mathematics for Games Developers"
     * page 144. */
    Mat4<float> adjustMatrix(
        x*x*(1-cosf(c))+cosf(c),    x*(1-cosf(c))*y+z*sinf(c),  z*x*(1-cosf(c))-y*sinf(c),  0.0f,
        x*(1-cosf(c))*y-z*sinf(c),  y*y*(1-cosf(c))+cosf(c),    z*y*(1-cosf(c))+x*sinf(c),  0.0f,
        z*(1-cosf(c))*x+y*sinf(c),  z*(1-cosf(c))*y-x*sinf(c),  z*z*(1-cosf(c))+cosf(c),    0.0f,
        0.0f,                       0.0f,                       0.0f,                       1.0f
    );

    /* Concatenate this transformation onto the current overall
     * rotation.  (It's the last one applied, so it goes first.)
     * TODO: Am I eventually going to have a loss-of-precision problem?
     */
    rotateMatrix = adjustMatrix * rotateMatrix;

    /* Re-orient all the axes with the new rotation matrix.
     * It's important that I do this rather than adjust them with
     * adjustMatrix so that the axes don't drift from the rotation
     * with accumulated errors. */
    initAxes();
#else
    switch (axis)
    {
    case AXIS_PITCH:
        rotateMatrix = Mat4<float>(
            1.0f,   0.0f,       0.0f,       0.0f,
            0.0f,   cosf(c),    sinf(c),    0.0f,
            0.0f,   -sinf(c),   cosf(c),    0.0f,
            0.0f,   0.0f,       0.0f,       1.0f) * rotateMatrix;
        break;

    case AXIS_YAW:
        rotateMatrix = Mat4<float>(
            cosf(c),    0.0f,   -sinf(c),   0.0f,
            0.0f,       1.0f,   0.0f,       0.0f,
            sinf(c),    0.0f,   cosf(c),    0.0f,
            0.0f,       0.0f,   0.0f,       1.0f) * rotateMatrix;
        break;

    case AXIS_ROLL:
        rotateMatrix = Mat4<float>(
            cosf(c),    sinf(c),    0.0f,   0.0f,
            -sinf(c),   cosf(c),    0.0f,   0.0f,
            0.0f,       0.0f,       1.0f,   0.0f,
            0.0f,       0.0f,       0.0f,   1.0f) * rotateMatrix;
        break;
    }
#endif /* ARBITRARY_AXIS_DOODAH */
}

void AFK_Object::displace(enum AFK_Axes axis, float change)
{
#ifdef ARBITRARY_AXIS_DOODAH
    translate += Vec3<float>(
        axes[axis].v[0] / axes[axis].v[3],
        axes[axis].v[1] / axes[axis].v[3],
        axes[axis].v[2] / axes[axis].v[3]) * change;
#else
    /* This is not a displacement along one of the three basic
     * axes!  I need to apply the object's current rotation
     * to them to work out the actual desired direction of
     * displacement.
     */
    Vec4<float> axisVec4;

    switch (axis)
    {
    case AXIS_PITCH:
        axisVec4 = rotateMatrix * Vec4<float>(1.0f, 0.0f, 0.0f, 1.0f);
        break;

    case AXIS_YAW:
        axisVec4 = rotateMatrix * Vec4<float>(0.0f, 1.0f, 0.0f, 1.0f);
        break;

    case AXIS_ROLL:
        axisVec4 = rotateMatrix * Vec4<float>(0.0f, 0.0f, 1.0f, 1.0f);
        break;
    }

    translate += Vec3<float>(
        axisVec4.v[0] / axisVec4.v[3],
        axisVec4.v[1] / axisVec4.v[3],
        axisVec4.v[2] / axisVec4.v[3]) * change;
#endif /* ARBITRARY_AXIS_DOODAH */
}

Mat4<float> AFK_Object::getTranslation() const
{
    return Mat4<float>(
        1.0f,       0.0f,       0.0f,       translate.v[0],
        0.0f,       1.0f,       0.0f,       translate.v[1],
        0.0f,       0.0f,       1.0f,       translate.v[2],
        0.0f,       0.0f,       0.0f,       1.0
    );
}

Mat4<float> AFK_Object::getRotation() const
{
    return rotateMatrix;
}

Mat4<float> AFK_Object::getScaling() const
{
    return Mat4<float>(
        scale.v[0], 0.0f,       0.0f,       0.0f,
        0.0f,       scale.v[1], 0.0f,       0.0f,
        0.0f,       0.0f,       scale.v[2], 0.0f,
        0.0f,       0.0f,       0.0f,       1.0
    );
}

