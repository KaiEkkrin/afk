/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_CORE_H_
#define _AFK_CORE_H_

#include "afk.hpp"

#include <iostream>
#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lockfree/queue.hpp>

#include "camera.hpp"
#include "config.hpp"
#include "def.hpp"
#include "display.hpp"
#include "frame.hpp"
#include "landscape.hpp"
#include "light.hpp"
#include "rng/rng.hpp"


void afk_idle(void);

class AFK_Core
{
protected:
    /* These are part of the render process managed by afk_idle().
     * Nothing else ought to be looking at them.
     */

    /* The result we're currently waiting on from the computing
     * side of things, if there is one.
     */
    boost::unique_future<bool> computingUpdate;
    bool computingUpdateDelayed;
    unsigned int delaysSinceLastCalibration;
    unsigned int delaysSinceLastCheckpoint;

    boost::posix_time::ptime lastCalibration;

    /* This stuff is for the OpenGL buffer cleanup, glBuffersForDeletion()
     * etc.
     */
    boost::lockfree::queue<GLuint> glGarbageBufs;

    void deleteGlGarbageBufs(void);

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

    /* TODO Compute for the previous frame while rendering the
     * current one, and track a "computingFrame" as well, here
     * and maybe even more at some point! :P
     * Although I only really seem to need that to manage
     * cache eviction...
     */

    /* This is the frame currently being rendered. */
    AFK_Frame           renderingFrame;

    /* This is the frame currently being computed.  It's meant
     * to track one step behind the frame currently being
     * rendered, but if the computing process doesn't finish
     * in time, it'll lag behind and then jump
     */
    AFK_Frame           computingFrame;

    /* The camera. */
    AFK_Camera          *camera;

    /* The landscape. */
    AFK_Landscape       *landscape;

    /* Global lighting. */
    AFK_Light           sun;

    /* To track various special objects. */
    AFK_DisplayedProtagonist        *protagonist;

    /* Input state. */
    /* This vector is (right thrusters, up thrusters, throttle). */
    Vec3<float>         velocity;

    /* This vector is control axis displacement: (pitch, yaw, roll). */
    Vec3<float>         axisDisplacement;

    /* Bits set/cleared based on AFK_Controls */
    unsigned long long  controlsEnabled;

    /* This buffer holds the last frame's worth of occasional
     * prints, so that I can dump them if we quit.
     * TODO Make the entire mechanism contingent on some
     * preprocessor variable?  It will slow AFK down.
     */
    std::ostringstream occasionalPrints;

    /* For tracking and occasionally printing what the engine is
     * doing.
     */
    boost::posix_time::ptime lastCheckpoint;
    AFK_Frame           frameAtLastCheckpoint;   

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

    /* This utility function prints a message at checkpoints,
     * so that I can usefully debug-print
     * engine state without spamming stdout.
     * TODO So that I'm not wasting time formatting strings
     * that get thrown away, change checkpoint() to call into
     * various bits of AFK asking for the information as
     * required?
     */
    void occasionallyPrint(const std::string& message);

    /* Does a checkpoint.  This happens once every
     * checkpoint interval unless `definitely' is set
     * in which case it happens right away.
     */
    void checkpoint(boost::posix_time::ptime& now, bool definitely);

    /* TODO REMOVE WHEN LOSE DEPENDENCY ON GLUT
     * Because GLUT requires that all OpenGL commands be called
     * from the primary thread, deletes requested on other
     * threads should push their gl buffers to here to be
     * cleaned up in the main loop.
     */
    void glBuffersForDeletion(GLuint *bufs, size_t bufsSize);

    friend void afk_idle(void);
};

extern AFK_Core afk_core;

#endif /* _AFK_CORE_H_ */

