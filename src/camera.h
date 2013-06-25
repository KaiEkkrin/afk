/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CAMERA_H_
#define _AFK_CAMERA_H_

#include "object.h"

/* The Camera is a kind of Object.  It moves the same way. */

class AFK_Camera: public AFK_Object
{
public:
    /* Basic parameters. */
    int     windowWidth;
    int     windowHeight;

    /* Intermediate results I keep around. */
    float   ar;
    float   tanHalfFov;

    /* The vector that separates the lens from the drive point.
     * (0,0,0) gives first person perspective.  Something with
     * negative z gives third person. */
    Vec3<float> separation;

    AFK_Camera();

    /* Need to call this before the camera is properly set up */
    void setWindowDimensions(int width, int height);

    /* Camera displacement is inverted */
    void adjustAttitude(enum AFK_Axes axis, float c);
    void displace(const Vec3<float>& v);

    Mat4<float> getProjection() const;
};

#endif /* _AFK_CAMERA_H_ */

