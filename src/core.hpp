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

#ifndef _AFK_CORE_H_
#define _AFK_CORE_H_

#include "afk.hpp"

#include <iostream>
#include <string>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/thread/future.hpp>

#include "async/thread_allocation.hpp"
#include "camera.hpp"
#include "computer.hpp"
#include "config.hpp"
#include "data/claimable.hpp"
#include "data/evictable_cache.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "display.hpp"
#include "light.hpp"
#include "window.hpp"


/* Forward declare a pile of stuff, to avoid this header file depending
 * on everything in existence just because I'm declaring pointers to
 * named classes
 */
class AFK_Camera;
class AFK_Config;
class AFK_Computer;
class AFK_RNG;
class AFK_World;


void afk_idle(void);

class AFK_Core
{
protected:
    /* These are part of the render process managed by afk_displayLoop().
     * Nothing else ought to be looking at them.
     */

    /* The result we're currently waiting on from the computing
     * side of things, if there is one.
     */
    boost::unique_future<bool> computingUpdate;
    bool computingUpdateDelayed;
    unsigned int computeDelaysSinceLastCheckpoint;
    unsigned int graphicsDelaysSinceLastCheckpoint;

    boost::posix_time::ptime startOfFrameTime;
    boost::posix_time::ptime lastFrameTime;
    boost::posix_time::ptime lastCalibration;
    unsigned int graphicsDelaysSinceLastCalibration;

    /* The calibration error is the number of microseconds away
     * I was from filling all the frame time with calculation.
     * Negative means I finished early, positive means late.
     */
    int calibrationError;

    /* This stuff is for the OpenGL buffer cleanup, glBuffersForDeletion()
     * etc.
     */
    boost::lockfree::queue<GLuint> glGarbageBufs;

    void deleteGlGarbageBufs(void);

public:
    /* General things. */
    AFK_Config          *config;
    AFK_Computer        *computer;
    unsigned int        masterThreadId;
    AFK_ThreadAllocation    threadAlloc;
    AFK_Window          *window;

    /* This RNG is used only for setting things up. */
    AFK_RNG             *rng;

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

    /* The world. */
    AFK_World           *world;

    /* Global lighting. */
    AFK_Light           sun;

    /* Base colour. */
    Vec3<float>         skyColour;

    /* To track various special objects. */
    AFK_DisplayedProtagonist        *protagonist;

    /* Input state. */
    /* This vector is (right thrusters, up thrusters, throttle). */
    Vec3<float>         velocity;

    /* This vector is control axis displacement: (pitch, yaw, roll). */
    Vec3<float>         axisDisplacement;

    /* Bits set/cleared based on AFK_Controls */
    uint64_t            controlsEnabled;

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
    void configure(int *argcp, char **argv);
    void initGraphics(void);
    
    void loop(void);

    /* For object updates. */
    const boost::posix_time::ptime& getStartOfFrameTime(void) const;

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

    /* TODO NASTY -- Share GL context between all threads and remove
     * Deletes of GL objects from other threads go into here so that
     * the main thread (with the GL context) can clean up.
     */
    void glBuffersForDeletion(GLuint *bufs, size_t bufsSize);

    friend void afk_idle(void);
};

extern AFK_Core afk_core;

/* This function accesses the computing frame counter in the AFK core.
 * Used by the caches to track entry age.
 */
extern AFK_GetComputingFrame afk_getComputingFrameFunc;

/* Here's a shorthand for claim types with that frame counter
 * getter function
 */
#define AFK_CLAIM_OF(t) AFK_Claim<AFK_##t, afk_getComputingFrameFunc>

/* I'll define the caches here, it's a nice central place to put them.
 * I'm going to use plenty of hash bits for the world cache (many many
 * small cells).  The others will have fewer.  These are hardwired
 * tweakables; check the memory usage.
 */
#define AFK_WORLD_CACHE AFK_EvictableCache<AFK_Cell, AFK_WorldCell, AFK_HashCell, afk_unassignedCell, 22, 60, afk_getComputingFrameFunc>
#define AFK_LANDSCAPE_CACHE AFK_EvictableCache<AFK_Tile, AFK_LandscapeTile, AFK_HashTile, afk_unassignedTile, 16, 10, afk_getComputingFrameFunc>
#define AFK_SHAPE_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_ShapeCell, AFK_HashKeyedCell, afk_unassignedKeyedCell, 16, 10, afk_getComputingFrameFunc>
#define AFK_VAPOUR_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_VapourCell, AFK_HashKeyedCell, afk_unassignedKeyedCell, 16, 10, afk_getComputingFrameFunc>

#endif /* _AFK_CORE_H_ */

