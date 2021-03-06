/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include <future>
#include <iostream>
#include <string>

#include <boost/lockfree/queue.hpp>

#include "async/thread_allocation.hpp"
#include "camera.hpp"
#include "clock.hpp"
#include "computer.hpp"
#include "data/claimable.hpp"
#include "data/evictable_cache.hpp"
#include "data/frame.hpp"
#include "def.hpp"
#include "detail_adjuster.hpp"
#include "display.hpp"
#include "light.hpp"
#include "ui/config_settings.hpp"
#include "window.hpp"


/* Forward declare a pile of stuff, to avoid this header file depending
 * on everything in existence just because I'm declaring pointers to
 * named classes
 */
class AFK_Camera;
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
    std::future<bool> computingUpdate;
    bool computingUpdateDelayed;

    AFK_DetailAdjuster *detailAdjuster;

    /* This stuff is for the OpenGL buffer cleanup, glBuffersForDeletion()
     * etc.
     */
    boost::lockfree::queue<GLuint> glGarbageBufs;

    void deleteGlGarbageBufs(void);

public:
    /* General things. */
    AFK_ConfigSettings  settings;
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

    /* The camera.  This goes to the worker threads as a thread-local. */
    AFK_Camera          camera;

    /* The world.  This does too. */
    AFK_World           *world;

    /* Global lighting. */
    AFK_Light           sun;

    /* Base colour. */
    Vec3<float>         skyColour;

    /* To track various special objects. */
    AFK_DisplayedProtagonist        protagonist;

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
    afk_clock::time_point   lastCheckpoint;
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
    void checkpoint(const afk_clock::time_point& now, bool definitely);

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

/* I'll define the caches here, it's a nice central place to put them.
 * I'm going to use plenty of hash bits for the world cache (many many
 * small cells).  The others will have fewer.  These are hardwired
 * tweakables; check the memory usage.
 */

/* TODO: To verify the initialisation stuff, making some of the caches
 * temporarily really small.  Better hashBits values for world and
 * landscape are 22 and 16 respectively
 */
#define AFK_WORLD_CACHE AFK_EvictableCache<AFK_Cell, AFK_WorldCell, AFK_HashCell, afk_unassignedCell, 20, 60, afk_getComputingFrameFunc>
#define AFK_LANDSCAPE_CACHE AFK_EvictableCache<AFK_Tile, AFK_LandscapeTile, AFK_HashTile, afk_unassignedTile, 16, 10, afk_getComputingFrameFunc>
#define AFK_SHAPE_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_ShapeCell, AFK_HashKeyedCell, afk_unassignedKeyedCell, 16, 10, afk_getComputingFrameFunc>
#define AFK_VAPOUR_CELL_CACHE AFK_EvictableCache<AFK_KeyedCell, AFK_VapourCell, AFK_HashKeyedCell, afk_unassignedKeyedCell, 16, 10, afk_getComputingFrameFunc>

#endif /* _AFK_CORE_H_ */

