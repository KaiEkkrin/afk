/* AFK
 * Copyright (C) 2013, Alex Holloway.
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

#ifndef _AFK_CAMERA_H_
#define _AFK_CAMERA_H_

#include "def.hpp"
#include "object.hpp"

class AFK_Camera: protected AFK_Object
{
protected:
    /* Basic parameters. */
    float   zNear;
    float   zFar;
    int     windowWidth;
    int     windowHeight;

    /* The vector that separates the lens from the drive point.
     * (0,0,0) gives first person perspective.  Something with
     * negative z gives third person. */
    Vec3<float> separation;

    /* Intermediate results I keep around. */
    float   ar;
    float   tanHalfFov;

    /* Cache the projection here.  It gets requested an awful
     * lot in order to test cell visibility; if I don't do this
     * I end up spending about 1/3 of my time just doing
     * matrix multiplication!
     */
    Vec2<float> windowSize;
    Mat4<float> projection;

    void updateProjection(void);

public:
    AFK_Camera();
    virtual ~AFK_Camera();

    /* Need to call these before the camera is properly set up */
    void setSeparation(const Vec3<float>& _separation);
    void setWindowDimensions(int width, int height);

    /* For testing an object against a target detail pitch. */
    /* TODO Why am I tracking viewer location by protagonist
     * (with that separation hack !  clearly wrong) rather than having the
     * Camera track the viewer location?
     */
    float getDetailPitchAsSeen(float objectScale, const Vec3<float>& objectLocation, const Vec3<float>& viewerLocation) const;

    /* Checks whether a projected point will be visible or not. */
    bool projectedPointIsVisible(const Vec4<float>& projectedPoint) const;

    void driveAndUpdateProjection(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement);
    Vec2<float> getWindowSize(void) const { return windowSize; }
    Mat4<float> getProjection(void) const { return projection; }
};

#endif /* _AFK_CAMERA_H_ */

