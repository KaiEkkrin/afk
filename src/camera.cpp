/* AFK (c) Alex Holloway 2013 */

#include "afk.h"

#include <math.h>

#include "camera.h"
#include "config.h"
#include "core.h"

AFK_Camera::AFK_Camera(): AFK_Object()
{
    separation = Vec3<float>(0.0f, 0.0f, 0.0f);
}

void AFK_Camera::setWindowDimensions(int width, int height)
{
    windowWidth  = width;
    windowHeight = height;
}

/* The camera's transformations need to be inverted and to be
 * composed the opposite way round. */
void AFK_Camera::adjustAttitude(enum AFK_Axes axis, float c)
{
    switch (axis)
    {
    case AXIS_PITCH:
        movement = Mat4<float>(
            1.0f,   0.0f,       0.0f,       0.0f,
            0.0f,   cosf(-c),   sinf(-c),   0.0f,
            0.0f,   -sinf(-c),  cosf(-c),   0.0f,
            0.0f,   0.0f,       0.0f,       1.0f) * movement;
        break;

    case AXIS_YAW:
        movement = Mat4<float>(
            cosf(-c),   0.0f,   -sinf(-c),  0.0f,
            0.0f,       1.0f,   0.0f,       0.0f,
            sinf(-c),   0.0f,   cosf(-c),   0.0f,
            0.0f,       0.0f,   0.0f,       1.0f) * movement;
        break;

    case AXIS_ROLL:
        movement = Mat4<float>(
            cosf(-c),   sinf(-c),   0.0f,   0.0f,
            -sinf(-c),  cosf(-c),   0.0f,   0.0f,
            0.0f,       0.0f,       1.0f,   0.0f,
            0.0f,       0.0f,       0.0f,   1.0f) * movement;
        break;
    }
}

void AFK_Camera::displace(const Vec3<float>& v)
{
    movement = Mat4<float>(
        1.0f,   0.0f,   0.0f,   -v.v[0],
        0.0f,   1.0f,   0.0f,   -v.v[1],
        0.0f,   0.0f,   1.0f,   -v.v[2],
        0.0f,   0.0f,   0.0f,   1.0f) * movement;
}

Mat4<float> AFK_Camera::getProjection() const
{
    /* Magic perspective projection. */
    float ar = ((float)windowWidth) / ((float)windowHeight);
    float fov = afk_core.config->fov;
    float zNear = afk_core.config->zNear;
    float zFar = afk_core.config->zFar;
    float zRange = zNear - zFar;
    float tanHalfFov = tanf((fov / 2.0f) * M_PI / 180.0f);
    
    Mat4<float> projectMatrix(
        1.0f / (tanHalfFov * ar),   0.0f,                   0.0f,                       0.0f,
        0.0f,                       1.0f / tanHalfFov,      0.0f,                       0.0f,
        0.0f,                       0.0f,                   (-zNear - zFar) / zRange,   2.0f * zFar * zNear / zRange,
        0.0f,                       0.0f,                   1.0f,                       0.0f
    );

    /* The separation transform for 3rd person perspective. */
    Mat4<float> separationMatrix(
        1.0f,   0.0f,   0.0f,   separation.v[0],
        0.0f,   1.0f,   0.0f,   separation.v[1],
        0.0f,   0.0f,   1.0f,   separation.v[2],
        0.0f,   0.0f,   0.0f,   1.0f);

    return projectMatrix * separationMatrix * getTransformation();
}

