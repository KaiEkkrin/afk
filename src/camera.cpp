/* AFK (c) Alex Holloway 2013 */

#include <math.h>

#include "camera.h"

Mat4<float> AFK_Camera::getProjection() const
{
    /* Magic perspective projection. */
    float ar = ((float)windowWidth) / ((float)windowHeight);
    float zRange = zNear - zFar;
    float tanHalfFov = tanf((fov / 2.0f) * M_PI / 180.0f);
    
    Mat4<float> projectMatrix(
        1.0f / (tanHalfFov * ar),   0.0f,                   0.0f,                       0.0f,
        0.0f,                       1.0f / tanHalfFov,      0.0f,                       0.0f,
        0.0f,                       0.0f,                   (-zNear - zFar) / zRange,   2.0f * zFar * zNear / zRange,
        0.0f,                       0.0f,                   1.0f,                       0.0f
    );

    return projectMatrix * getTranslation() * getRotation() * getScaling();
}

