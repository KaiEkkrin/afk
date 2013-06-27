/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <math.h>

#include "camera.hpp"
#include "config.hpp"
#include "core.hpp"

AFK_Camera::AFK_Camera(Vec3<float> _separation): AFK_Object()
{
    separation = _separation;
}

void AFK_Camera::setWindowDimensions(int width, int height)
{
    windowWidth  = width;
    windowHeight = height;

    /* If we're setting up a window, this stuff must be
     * valid by now
     */
    tanHalfFov = tanf((afk_core.config->fov / 2.0f) * M_PI / 180.0f);
    ar = ((float)windowWidth) / ((float)windowHeight);
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
    float zNear = afk_core.config->zNear;
    float zFar = afk_core.config->zFar;
    float zRange = zNear - zFar;
    
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

