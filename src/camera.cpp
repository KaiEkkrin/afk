/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see [http://www.gnu.org/licenses/].
 */

#include "afk.hpp"

#include <algorithm>
#include <cmath>

#include "camera.hpp"
#include "core.hpp"
#include "ui/config_settings.hpp"

void AFK_Camera::updateProjection(void)
{
    /* Magic perspective projection. */
    zNear = afk_core.settings.zNear;
    zFar = afk_core.settings.zFar;
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

    /* We then apply the Object transform, in reverse.  (Ignoring
     * scale.)
     */
    AFK_Object reverseObject(scale, -translation, rotation.inverse());
    projection = projectMatrix * separationMatrix *
        reverseObject.getRotationMatrix() *
        reverseObject.getTranslationMatrix();
}

AFK_Camera::AFK_Camera():
    AFK_Object()
{
}

AFK_Camera::~AFK_Camera()
{
}

void AFK_Camera::setSeparation(const Vec3<float>& _separation)
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
    tanHalfFov = tanf((afk_core.settings.fov / 2.0f) * (float)M_PI / 180.0f);
    ar = ((float)windowWidth) / ((float)windowHeight);

    windowSize = afk_vec2<float>((float)windowWidth, (float)windowHeight);
}

float AFK_Camera::getDetailPitchAsSeen(
    float objectScale,
    const Vec3<float>& objectLocation,
    const Vec3<float>& viewerLocation) const
{
    float distanceToViewer = (objectLocation - (viewerLocation - separation)).magnitude();
    return windowHeight * objectScale / (tanHalfFov *
        std::max<float>(std::min<float>(distanceToViewer, zFar), zNear));
}

bool AFK_Camera::projectedPointIsVisible(const Vec4<float>& projectedPoint) const
{
    return (
        (projectedPoint.v[0] / projectedPoint.v[2]) >= -ar &&
        (projectedPoint.v[0] / projectedPoint.v[2]) <= ar &&
        (projectedPoint.v[1] / projectedPoint.v[2]) >= -1.0f &&
        (projectedPoint.v[1] / projectedPoint.v[2]) <= 1.0f);
}

void AFK_Camera::driveAndUpdateProjection(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement)
{
    drive(velocity, axisDisplacement);
    updateProjection();
}

