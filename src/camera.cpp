/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include "camera.h"
#include "config.h"
#include "state.h"

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

void AFK_Camera::displace(enum AFK_Axes axis, float c)
{
    movement = Mat4<float>(
        1.0f,   0.0f,   0.0f,   axis == AXIS_PITCH ? -c: 0.0f,
        0.0f,   1.0f,   0.0f,   axis == AXIS_YAW ? -c: 0.0f,
        0.0f,   0.0f,   1.0f,   axis == AXIS_ROLL ? -c: 0.0f,
        0.0f,   0.0f,   0.0f,   1.0f) * movement;
}

Mat4<float> AFK_Camera::getProjection() const
{
    /* Magic perspective projection.
     * TODO This magic is giving me a rather squeezed projection with a
     * widescreen window!  Can I change it so that the X scaling == the
     * Y scaling always, so that the X and Y fov's are only equal when
     * the window is square? */
    float ar = ((float)windowWidth) / ((float)windowHeight);
    float fov = afk_state.config->fov;
    float zNear = afk_state.config->zNear;
    float zFar = afk_state.config->zFar;
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

