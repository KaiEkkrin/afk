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

    /* Intermediate results I keep around. */
    float   ar;
    float   tanHalfFov;

    /* Cache the projection here.  It gets requested an awful
     * lot in order to test cell visibility; if I don't do this
     * I end up spending about 1/3 of my time just doing
     * matrix multiplication!
     */
    Mat4<float> projection;

    void updateProjection(void);

public:
    /* The vector that separates the lens from the drive point.
     * (0,0,0) gives first person perspective.  Something with
     * negative z gives third person. */
    const Vec3<float> separation;

    AFK_Camera(Vec3<float> _separation);
    virtual ~AFK_Camera();

    /* Need to call this before the camera is properly set up */
    void setWindowDimensions(int width, int height);

    /* For testing an object against a target detail pitch. */
    float getDetailPitchAsSeen(float scale, float distanceToViewer) const;

    /* Checks whether a projected point will be visible or not. */
    bool projectedPointIsVisible(const Vec4<float>& projectedPoint) const;

    void driveAndUpdateProjection(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement);
    Mat4<float> getProjection(void) const;
};

#endif /* _AFK_CAMERA_H_ */

