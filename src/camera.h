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

    AFK_Camera();

    /* Need to call this before the camera is properly set up */
    void setWindowDimensions(int width, int height);

    /* Drives the camera about, based on current input state. */
    void drive();

    Mat4<float> getProjection() const;
};

#endif /* _AFK_CAMERA_H_ */

