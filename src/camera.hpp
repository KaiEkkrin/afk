/* AFK (c) Alex Holloway 2013 */

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

