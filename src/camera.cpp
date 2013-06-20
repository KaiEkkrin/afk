/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include "camera.h"
#include "config.h"
#include "state.h"

AFK_Camera::AFK_Camera(): AFK_Object()
{
    /* Start it a bit back so that I can see the origin */
    translate = Vec3<float>(0.0f, 0.0f, 5.0f);
}

void AFK_Camera::setWindowDimensions(int width, int height)
{
    windowWidth  = width;
    windowHeight = height;
}

void AFK_Camera::drive(void)
{
    /* In all cases I'm going to treat the axes individually.
     * This means theoretically doing lots more matrix multiplies,
     * but in practice if I tried to combine them I'd get a
     * headache and probably also have to do square roots, which
     * are no doubt more expensive.
     */
    adjustAttitude(AXIS_PITCH,  afk_state.axisDisplacement.v[0]);
    adjustAttitude(AXIS_YAW,    afk_state.axisDisplacement.v[1]);
    adjustAttitude(AXIS_ROLL,   afk_state.axisDisplacement.v[2]);

    displace(AXIS_PITCH,        -afk_state.velocity.v[0]);
    displace(AXIS_YAW,          -afk_state.velocity.v[1]);
    displace(AXIS_ROLL,         -afk_state.velocity.v[2]);
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

    /* Camera rotation and scaling mean rotating and scaling the world
     * _around the place the camera is_, not around the origin,
     * hence the inversion of the multiplication order here!
     */
    return projectMatrix * getScaling() * getRotation() * getTranslation();
}

