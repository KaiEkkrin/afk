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

#include "afk.hpp"

#include <cmath>

#include <boost/random/random_device.hpp>

#include "camera.hpp"
#include "computer.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "file/logstream.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng.hpp"
#include "test_jigsaw.hpp"
#include "window_glx.hpp"
#include "window_wgl.hpp"
#include "world.hpp"


#define FRAME_NUMBER_DEBUG 0


/* With a little less for wiggle room. */
#define FRAME_REFRESH_TIME_MILLIS 15.5f


#define JIGSAW_TEST 0


#define CALIBRATION_INTERVAL_MILLIS (afk_core.config->targetFrameTimeMillis * afk_core.config->framesPerCalibration)

void afk_displayInit(void)
{
}

void afk_idle(void)
{
    afk_core.detailAdjuster->startOfFrame();
    afk_core.checkpoint(afk_core.detailAdjuster->getStartOfFrameTime(), false);

    /* Always update the controls: I think that things ought
     * to feel smoother if they continue registering movement
     * changes during glitches (worth a try, anyway).
     * The compute phase gets its own copies of the camera and
     * protagonist, so I'm not about to mess them about.
     */
    float frameInterval;
    if (afk_core.detailAdjuster->getFrameInterval(frameInterval))
    {
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::THRUST_RIGHT))
            afk_core.velocity.v[0] += afk_core.settings.thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::THRUST_LEFT))
            afk_core.velocity.v[0] -= afk_core.settings.thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::THRUST_UP))
            afk_core.velocity.v[1] += afk_core.settings.thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::THRUST_DOWN))
            afk_core.velocity.v[1] -= afk_core.settings.thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::THRUST_FORWARD))
            afk_core.velocity.v[2] += afk_core.settings.thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::THRUST_BACKWARD))
            afk_core.velocity.v[2] -= afk_core.settings.thrustButtonSensitivity * frameInterval;
        
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::PITCH_UP))
            afk_core.axisDisplacement.v[0] += afk_core.settings.rotateButtonSensitivity * frameInterval * afk_core.settings.getAxisInversion(AFK_Control::AXIS_PITCH);
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::PITCH_DOWN))
            afk_core.axisDisplacement.v[0] -= afk_core.settings.rotateButtonSensitivity * frameInterval * afk_core.settings.getAxisInversion(AFK_Control::AXIS_PITCH);
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::YAW_RIGHT))
            afk_core.axisDisplacement.v[1] += afk_core.settings.rotateButtonSensitivity * frameInterval * afk_core.settings.getAxisInversion(AFK_Control::AXIS_YAW);
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::YAW_LEFT))
            afk_core.axisDisplacement.v[1] -= afk_core.settings.rotateButtonSensitivity * frameInterval * afk_core.settings.getAxisInversion(AFK_Control::AXIS_YAW);
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::ROLL_RIGHT))
            afk_core.axisDisplacement.v[2] += afk_core.settings.rotateButtonSensitivity * frameInterval * afk_core.settings.getAxisInversion(AFK_Control::AXIS_ROLL);
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::ROLL_LEFT))
            afk_core.axisDisplacement.v[2] -= afk_core.settings.rotateButtonSensitivity * frameInterval * afk_core.settings.getAxisInversion(AFK_Control::AXIS_ROLL);
        
        afk_core.camera.driveAndUpdateProjection(
            afk_core.velocity * frameInterval, afk_core.axisDisplacement);
        
        /* Protagonist follow camera.  (To make it look more
         * natural, at some point I want a stretchy leash)
         */
        afk_core.protagonist.object.drive(
            afk_core.velocity * frameInterval, afk_core.axisDisplacement);
        
        /* Swallow the axis displacement after it's been applied
         * so that I don't get a strange mouse acceleration effect
         */
        afk_core.axisDisplacement = afk_vec3<float>(0.0f, 0.0f, 0.0f);
        
        /* Manage the other objects.
         * TODO A call-through to AI stuff probably goes here?
         */
    }

    if (!afk_core.computingUpdateDelayed)
    {
        /* Update the world, deciding which bits of it I'm going
         * to draw.
         */
        afk_core.computingUpdate = afk_core.world->updateWorld(
            afk_core.camera, afk_core.protagonist.object, afk_core.detailAdjuster->getDetailPitch());

        /* Meanwhile, draw the previous frame */
        afk_display(afk_core.masterThreadId);
    }

    /* Clean up anything that's gotten queued into the garbage queue */
    /* TODO take this out, enable the workers to delete their own
     * GL garbage
     */
    afk_core.deleteGlGarbageBufs();

    /* Wait until it's about time to display the next frame. */
    afk_duration_mfl computeWaitTime = afk_core.detailAdjuster->getComputeWaitTime();
#ifdef _WIN32
    /* TODO: There appears to be an issue with the Visual Studio 2013
     * future implementation and this floating point duration value,
     * convert it to integer micros
     */
    std::chrono::microseconds computeWaitTimeMicros =
        std::chrono::duration_cast<std::chrono::microseconds>(computeWaitTime);
    std::future_status status = afk_core.computingUpdate.wait_for(computeWaitTimeMicros);
#else
    std::future_status status = afk_core.computingUpdate.wait_for(computeWaitTime);
#endif

    switch (status)
    {
    case std::future_status::ready:
        {
            afk_core.detailAdjuster->computeFinished();

            /* Flip the buffers and bump the computing frame */
            afk_core.window->swapBuffers();
            afk_core.renderingFrame = afk_core.computingFrame;
            afk_core.computingFrame.increment();
            afk_core.world->flipRenderQueues(afk_core.computingFrame);
            afk_core.computingUpdateDelayed = false;

#if FRAME_NUMBER_DEBUG || AFK_SHAPE_ENUM_DEBUG
            AFK_DEBUG_PRINTL("Now computing frame " << afk_core.computingFrame);
#endif
        
            break;
        }

    case std::future_status::timeout:
        {
            /* Flag this update as delayed. */
            afk_core.computingUpdateDelayed = true;
            afk_core.detailAdjuster->computeTimedOut();
            break;
        }

    default:
        throw AFK_Exception("Unexpected future_status received");
    }
}


void AFK_Core::deleteGlGarbageBufs(void)
{
    static std::vector<GLuint> bufs;
    GLuint buf;
    while (glGarbageBufs.pop(buf))
        bufs.push_back(buf);

    if (bufs.size() > 0)
        glDeleteBuffers(static_cast<GLsizei>(bufs.size()), &bufs[0]);
    bufs.clear();
}

/* The definition of AFK_Core itself. */

AFK_Core::AFK_Core():
    computingUpdateDelayed(false),
    detailAdjuster(nullptr),
    glGarbageBufs(1000),
    computer(nullptr),
    window(nullptr),
    rng(nullptr),
    world(nullptr)
{
    masterThreadId = threadAlloc.getNewId();
}

AFK_Core::~AFK_Core()
{
    /* I need to be a bit careful about the order I do
     * this in ...
     */
    if (window) afk_core.window->closeWindow();
    if (world) delete world;
    if (rng) delete rng;

    if (computer) delete computer; /* Should close CL contexts */
    if (detailAdjuster) delete detailAdjuster;
    if (window)
    {
        afk_out << "AFK: Destroying window" << std::endl;
        delete window; /* Should close GL contexts */
    }
}

void AFK_Core::configure(int *argcp, char **argv)
{
    /* Measure the clock tick interval and make sure it's somewhere near
     * sane ...
     */
    afk_clock::time_point intervalTestStart = afk_clock::now();
    afk_clock::time_point intervalTestEnd = afk_clock::now();
    while (intervalTestStart == intervalTestEnd)
    {
        intervalTestEnd = afk_clock::now();
    }

    afk_duration_mfl tickInterval = std::chrono::duration_cast<afk_duration_mfl>(intervalTestEnd - intervalTestStart);
    afk_out << "AFK: Using clock with apparent tick interval: " << tickInterval.count() << " millis" << std::endl;
    assert(tickInterval.count() < 0.1f);

    if (!settings.parseCmdLine(argcp, argv))
    {
        throw AFK_Exception("Failed to parse command line");
    }

    rng = new AFK_Boost_Taus88_RNG();

    /* The special value -1 means no seed has been supplied, so I need to make one.
     * And of course, the seed comes in two 64-bit parts:
     */
    if (settings.masterSeedHigh == -1 || settings.masterSeedLow == -1)
    {
        boost::random_device rdev;
        if (settings.masterSeedHigh == -1)
        {
            settings.masterSeedHigh = (static_cast<int64_t>(rdev()) | (static_cast<int64_t>(rdev()) << 32));
        }

        if (settings.masterSeedLow == -1)
        {
            settings.masterSeedLow = (static_cast<int64_t>(rdev()) | (static_cast<int64_t>(rdev()) << 32));
        }
    }

    AFK_RNG_Value rSeed;
    rSeed.v.ll[0] = settings.masterSeedLow;
    rSeed.v.ll[1] = settings.masterSeedHigh;
    rng->seed(rSeed);

    /* Startup state of the protagonist. */
    velocity            = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    axisDisplacement    = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    controlsEnabled     = 0uLL;

    /* TODO Make the viewpoint configurable?  Right now I have a
     * fixed 3rd person view here.
     */
    camera.setSeparation(afk_vec3<float>(0.0f, -1.5f, 3.0f));

    /* Set up the sun.  (TODO: Make configurable?  Randomly
     * generated?  W/e :) )
     * TODO: Should I make separate ambient and diffuse colours,
     * and make the ambient colour dependent on the sky colour?
     */
    sun.colour = afk_vec3<float>(1.0f, 1.0f, 1.0f);
    sun.direction = afk_vec3<float>(-0.5f, -1.0f, 1.0f).normalise();
    sun.ambient = 0.2f;
    sun.diffuse = 1.0f;

    skyColour = afk_vec3<float>(
        rng->frand(), rng->frand(), rng->frand());
}

void AFK_Core::initGraphics(void)
{
#ifdef AFK_GLX
    window = new AFK_WindowGlx(settings.windowWidth, settings.windowHeight, settings.vsync);
#endif

#ifdef AFK_WGL
    window = new AFK_WindowWgl(settings.windowWidth, settings.windowHeight, settings.vsync);
#endif
}

void AFK_Core::loop(void)
{
    /* Shader setup. */
    afk_loadShaders(settings);

    computingFrame.increment();

    /* World setup. */
    computer = new AFK_Computer(settings);
    computer->loadPrograms(settings);

#if JIGSAW_TEST
    afk_testJigsaw(computer, config);
#endif

    /* I'll want this when I start running. */
    detailAdjuster = new AFK_DetailAdjuster(settings);

    /* Now that I've set that stuff up, find out how much memory
     * I have to play with ...
     */
    size_t clGlMaxAllocSize = computer->getFirstDeviceProps().maxMemAllocSize;
    afk_out << "AFK: Using GPU with " << std::dec << clGlMaxAllocSize << " bytes available to cl_gl";
    afk_out << " (" << clGlMaxAllocSize / (1024 * 1024) << "MB) global memory" << std::endl;

    /* Initialise the starting objects. */
    float worldMaxDistance = settings.zFar / 2.0f;

    world = new AFK_World(
        settings,
        computer,
        threadAlloc,
        worldMaxDistance,   /* maxDistance -- zFar must be a lot bigger or things will vanish */
        clGlMaxAllocSize / 4,
        clGlMaxAllocSize / 4,
        clGlMaxAllocSize / 4,
        rng);

    /* Make sure that camera is configured to match the window. */
    camera.setWindowDimensions(
        window->getWindowWidth(), window->getWindowHeight());

    /* TODO: Move the camera to somewhere above the landscape to
     * start with.
     * (A bit unclear how to best do this ... )
     */
    Vec3<float> startingMovement = afk_vec3<float>(0.0f, 8192.0f, 0.0f);
    Vec3<float> startingRotation = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    protagonist.object.drive(startingMovement, startingRotation);
    camera.driveAndUpdateProjection(startingMovement, startingRotation);

    /* Branch the main thread into the window event handling loop. */
    window->loopOnEvents(
        afk_idle,
        afk_keyboardUp,
        afk_keyboard,
        afk_mouseUp,
        afk_mouse,
        afk_motion);
}

void AFK_Core::occasionallyPrint(const std::string& message)
{
    occasionalPrints << "AFK Frame " << renderingFrame.get() << ": " << message << std::endl;
}

void AFK_Core::checkpoint(const afk_clock::time_point& now, bool definitely)
{
    afk_duration_mfl sinceLastCheckpoint = std::chrono::duration_cast<afk_duration_mfl>(
        now - lastCheckpoint);

    if ((definitely || sinceLastCheckpoint.count() >= 1000.0f) &&
        !(frameAtLastCheckpoint == renderingFrame))
    {
        lastCheckpoint = now;

        afk_out << "AFK: Checkpoint" << std::endl;
        afk_out << "AFK: Since last checkpoint: " << std::dec << renderingFrame - frameAtLastCheckpoint << " frames";
        float fps = (float)(renderingFrame - frameAtLastCheckpoint) * 1000.0f / sinceLastCheckpoint.count();
        afk_out << " (" << fps << " frames/second)" << std::endl;

        assert(detailAdjuster);
        detailAdjuster->checkpoint(sinceLastCheckpoint);

        assert(world);
        world->checkpoint(sinceLastCheckpoint);

        /* BODGE. This can cause a SIGSEGV when we do the final print upon
         * catching an exception, it looks like the destructors are
         * being called in a strange order
         */
        if (!definitely)
        {
            world->printCacheStats(afk_out, "Cache");
            world->printJigsawStats(afk_out, "Jigsaw");
        }

        afk_out << occasionalPrints.str();

        frameAtLastCheckpoint = renderingFrame;
    }

    occasionalPrints.str(std::string());
}

void AFK_Core::glBuffersForDeletion(GLuint *bufs, size_t bufsSize)
{
    for (size_t i = 0; i < bufsSize; ++i)
        glGarbageBufs.push(bufs[i]);
}

const AFK_Frame& afk_getComputingFrame(void)
{
    /* TODO Do I need atomic access to this?  (in which case I possibly
     * want to redefine AFK_Frame as inherently atomic rather than
     * continuing the current usage)
     */
    return afk_core.computingFrame;
}

AFK_GetComputingFrame afk_getComputingFrameFunc = afk_getComputingFrame;

