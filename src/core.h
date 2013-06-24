/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CORE_H_
#define _AFK_CORE_H_

#include "afk.h"

#include <vector>

#include "camera.h"
#include "config.h"
#include "def.h"
#include "display.h"

class AFK_Core
{
public:
    /* General things. */
    struct AFK_Config   *config;

    /* A list of all displayed objects.
     * TODO These should probably use a reference counting
     * pointer wrapper or something to simplify creating and
     * deleting lots of objects
     */
    std::vector<AFK_DisplayedObject *> dos;

    /* The camera. */
    AFK_Camera          camera;

    /* To track various special objects. */
    AFK_DisplayedTestObject         *testObject;
    AFK_DisplayedLandscapeObject    *landscapeObject;
    AFK_DisplayedProtagonist        *protagonist;

    /* Input state. */
    /* This vector is (right thrusters, up thrusters, throttle). */
    Vec3<float>         velocity;

    /* This vector is control axis displacement: (pitch, yaw, roll). */
    Vec3<float>         axisDisplacement;

    /* Bits set/cleared based on AFK_Controls */
    unsigned long long  controlsEnabled;

    AFK_Core();
    ~AFK_Core();

    /* The constructor doesn't do very much.  Call these things
     * to set up.  (In order!)
     * An AFK_Exception is thrown if stuff goes wrong.
     */
    void initGraphics(int *argcp, char **argv);
    //void initCompute(void);
    void configure(int *argcp, char **argv);
    
    void loop(void);
};

extern AFK_Core afk_core;

#endif /* _AFK_CORE_H_ */

