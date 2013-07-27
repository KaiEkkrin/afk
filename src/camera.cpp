/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <math.h>

#include "camera.hpp"
#include "config.hpp"
#include "core.hpp"

/* The camera's transformations need to be inverted and to be
 * composed the opposite way round. */
void AFK_Camera::adjustAttitude(enum AFK_Axes axis, float c)
{
    switch (axis)
    {
    case AXIS_PITCH:
        location = afk_mat4<float>(
            1.0f,   0.0f,       0.0f,       0.0f,
            0.0f,   cosf(-c),   sinf(-c),   0.0f,
            0.0f,   -sinf(-c),  cosf(-c),   0.0f,
            0.0f,   0.0f,       0.0f,       1.0f) * location;
        break;

    case AXIS_YAW:
        location = afk_mat4<float>(
            cosf(-c),   0.0f,   -sinf(-c),  0.0f,
            0.0f,       1.0f,   0.0f,       0.0f,
            sinf(-c),   0.0f,   cosf(-c),   0.0f,
            0.0f,       0.0f,   0.0f,       1.0f) * location;
        break;

    case AXIS_ROLL:
        location = afk_mat4<float>(
            cosf(-c),   sinf(-c),   0.0f,   0.0f,
            -sinf(-c),  cosf(-c),   0.0f,   0.0f,
            0.0f,       0.0f,       1.0f,   0.0f,
            0.0f,       0.0f,       0.0f,   1.0f) * location;
        break;
    }
}

void AFK_Camera::displace(const Vec3<float>& v)
{
    location = afk_mat4<float>(
        1.0f,   0.0f,   0.0f,   -v.v[0],
        0.0f,   1.0f,   0.0f,   -v.v[1],
        0.0f,   0.0f,   1.0f,   -v.v[2],
        0.0f,   0.0f,   0.0f,   1.0f) * location;
}

void AFK_Camera::updateProjection()
{
    /* Magic perspective projection. */
    float zNear = afk_core.config->zNear;
    float zFar = afk_core.config->zFar;
    float zRange = zNear - zFar;
    
    Mat4<float> projectMatrix = afk_mat4<float>(
        1.0f / (tanHalfFov * ar),   0.0f,                   0.0f,                       0.0f,
        0.0f,                       1.0f / tanHalfFov,      0.0f,                       0.0f,
        0.0f,                       0.0f,                   (-zNear - zFar) / zRange,   2.0f * zFar * zNear / zRange,
        0.0f,                       0.0f,                   1.0f,                       0.0f
    );

    /* The separation transform for 3rd person perspective. */
    Mat4<float> separationMatrix = afk_mat4<float>(
        1.0f,   0.0f,   0.0f,   separation.v[0],
        0.0f,   1.0f,   0.0f,   separation.v[1],
        0.0f,   0.0f,   1.0f,   separation.v[2],
        0.0f,   0.0f,   0.0f,   1.0f);

    projection = projectMatrix * separationMatrix * location;
}

AFK_Camera::AFK_Camera(Vec3<float> _separation): separation(_separation)
{
    location = afk_mat4<float>(
        1.0f,   0.0f,   0.0f,   0.0f,
        0.0f,   1.0f,   0.0f,   0.0f,
        0.0f,   0.0f,   1.0f,   0.0f,
        0.0f,   0.0f,   0.0f,   1.0f
    );

    updateProjection();
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

    updateProjection();
}

void AFK_Camera::drive(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement)
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
    updateProjection();
}

const Mat4<float>& AFK_Camera::getProjection() const
{
    return projection;
}

