/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_STATE_H_
#define _AFK_STATE_H_

#include <GL/gl.h>

#include "camera.h"
#include "config.h"
#include "def.h"

/* This is where I record AFK's global state.  I've got to do it like this,
 * because glut expects program state to be global (its callbacks give me
 * no way of passing a parameter.)
 */
struct AFK_State
{
    /* General things. */
    struct AFK_Config   *config;

    /* The camera. */
    AFK_Camera          camera;

    /* The protagonist object. */
    AFK_Object          *protagonist;

    /* Input state. */
    /* This vector is (right thrusters, up thrusters, throttle). */
    Vec3<float>         velocity;

    /* This vector is control axis displacement: (pitch, yaw, roll). */
    Vec3<float>         axisDisplacement;

    /* Bits set/cleared based on AFK_Controls */
    unsigned long long  controlsEnabled;
};

extern struct AFK_State afk_state;

#endif /* _AFK_STATE_H_ */

