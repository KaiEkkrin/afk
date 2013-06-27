/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CORE_H_
#define _AFK_CORE_H_

#include "afk.hpp"

#include <iostream>
#include <string>

#include "camera.hpp"
#include "config.hpp"
#include "def.hpp"
#include "display.hpp"
#include "landscape.hpp"
#include "rng/rng.hpp"

class AFK_Core
{
public:
    /* General things. */
    struct AFK_Config   *config;

    /* A wrapper around the compute engine. */
    struct AFK_Computer *computer;

    /* The random number generator.
     * TODO: The re-seeding process is serial, and I
     * don't want to be making these on the fly.
     * Therefore, if I shift to a threaded model (likely),
     * I need to be making one of these per worker thread
     * (in thread-local storage).
     */
    AFK_RNG             *rng;

    /* The camera. */
    AFK_Camera          *camera;

    /* The landscape. */
    AFK_Landscape       *landscape;

    /* To track various special objects. */
    AFK_DisplayedTestObject         *testObject;
    AFK_DisplayedProtagonist        *protagonist;

    /* Input state. */
    /* This vector is (right thrusters, up thrusters, throttle). */
    Vec3<float>         velocity;

    /* This vector is control axis displacement: (pitch, yaw, roll). */
    Vec3<float>         axisDisplacement;

    /* Bits set/cleared based on AFK_Controls */
    unsigned long long  controlsEnabled;

    /* This counter tracks which frame I'm displaying.
     * I'm going to want it.
     * Don't really need to worry about what happens
     * when it wraps.
     */
    unsigned int frameCounter;

    /* This buffer holds the last frame's worth of occasional
     * prints, so that I can dump them if we quit.
     * TODO Make the entire mechanism contingent on some
     * preprocessor variable?  It will slow AFK down.
     */
    std::ostringstream occasionalPrints;

    AFK_Core();
    ~AFK_Core();

    /* The constructor doesn't do very much.  Call these things
     * to set up.  (In order!)
     * An AFK_Exception is thrown if stuff goes wrong.
     */
    void initGraphics(int *argcp, char **argv);
    void initCompute(void);
    void configure(int *argcp, char **argv);
    
    void loop(void);

    /* This utility function prints a message once every certain
     * number of frames, so that I can usefully debug-print
     * engine state without spamming stdout.
     */
    void occasionallyPrint(const std::string& message);

    /* ...and the internal thing. */
    void printOccasionals(bool definitely);
};

extern AFK_Core afk_core;

#endif /* _AFK_CORE_H_ */

