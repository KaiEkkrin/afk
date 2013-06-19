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

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
            axes[i].v[j] = (i == j ? 1.0f : 0.0f);
        axes[i].v[3] = 1.0f;
    }

    rotateMatrix = Mat4<float>(
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   0.0f,
        0.0f,   0.0f,   0.0f,   1.0f
    );

    translate = Vec3<float>(0.0f, 0.0f, 0.0f);
}

void AFK_Object::adjustAttitude(int axis, float c)
{
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

    /* Rotate all three axes by this new rotation, too. */
    for (int i = 0; i < 3; ++i)
        axes[i] = adjustMatrix * axes[i];
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

