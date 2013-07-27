/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CAMERA_H_
#define _AFK_CAMERA_H_

#include "def.hpp"
#include "object.hpp"

class AFK_Camera
{
protected:
    /* This matrix represents where the camera is in space.
     * It's actually done like the Object transform matrix,
     * but inverted.
     * TODO: This causes us to track badly with an actual
     * Object.  I'd like to change this to an easily
     * invertable co-ordinate system to get rid of the
     * problem, but OMG, so hard
     */
    Mat4<float> location;

    /* The projection is wanted lots so cache it here. */
    Mat4<float> projection;

    /* Internal helpers that work like the Object functions. */
    void adjustAttitude(enum AFK_Axes axis, float c);
    void displace(const Vec3<float>& v);

    /* Worker function -- update the projection when
     * something has changed
     */
    void updateProjection();

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

    AFK_Camera(Vec3<float> _separation);
    virtual ~AFK_Camera() {}

    /* Need to call this before the camera is properly set up */
    void setWindowDimensions(int width, int height);

    /* Like the Object function. */
    void drive(const Vec3<float>& velocity, const Vec3<float>& axisDisplacement);

    const Mat4<float>& getProjection() const;
};

#endif /* _AFK_CAMERA_H_ */

