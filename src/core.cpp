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

#include "afk.hpp"

#include <cmath>

#include "camera.hpp"
#include "computer.hpp"
#include "config.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"
#include "rng/rng.hpp"
#include "test_jigsaw.hpp"
#include "window_glx.hpp"
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
#if 0
    afk_clock::time_point startOfFrameTime = afk_clock::now();

    /* If we just took less than FRAME_REFRESH_TIME for the entire
     * cycle, we're showing too little detail if we're not on a
     * Vsync system.
     */
    afk_duration_mfl wholeFrameTime = std::chrono::duration_cast<afk_duration_mfl>(
        startOfFrameTime - afk_core.startOfFrameTime);
    if (!afk_core.config->vsync && wholeFrameTime.count() < FRAME_REFRESH_TIME_MILLIS)
    {
        afk_core.calibrationError -= (FRAME_REFRESH_TIME_MILLIS - wholeFrameTime.count());
    }
    else
    {
        /* Work out what the graphics delay was. */
        afk_duration_mfl bufferFlipTime = std::chrono::duration_cast<afk_duration_mfl>(
            startOfFrameTime - afk_core.lastFrameTime);

        /* If it's been dropping frames, there will be more than a whole
         * number of `targetFrameTimeMicros' here.
         */
        float framesDropped = std::floor(bufferFlipTime.count() / afk_core.config->targetFrameTimeMillis);
        afk_core.graphicsDelaysSinceLastCheckpoint += framesDropped;
        afk_core.graphicsDelaysSinceLastCalibration += framesDropped;
    
        /* I'm going to tolerate one random graphics delay per calibration
         * point.  Glitches happen.
         */
        if (afk_core.graphicsDelaysSinceLastCalibration > 1)
        {
            afk_core.calibrationError += afk_core.config->targetFrameTimeMillis * framesDropped;
        }
    }

    afk_core.startOfFrameTime = startOfFrameTime;
#endif

    afk_core.checkpoint(afk_core.detailAdjuster->getStartOfFrameTime(), false);

#if 0
    /* Check whether I need to recalibrate. */
    /* TODO Make the time between calibrations configurable */
    afk_duration_mfl sinceLastCalibration = std::chrono::duration_cast<afk_duration_mfl>(
        startOfFrameTime - afk_core.lastCalibration);
    if (sinceLastCalibration.count() > CALIBRATION_INTERVAL_MILLIS)
    {
        /* Work out an error factor, and apply it to the LoD */
        float normalisedError = afk_core.calibrationError / CALIBRATION_INTERVAL_MILLIS; /* between -1 and 1? */

        /* Cap the normalised error: occasionally the graphics can
         * hang for ages and produce a spurious value which would
         * otherwise result in a negative detail factor, making us
         * crash
         */
        normalisedError = 0.5f * std::max(std::min(normalisedError, 1.0f), -1.0f);
        float detailFactor = -(1.0f / (normalisedError / 2.0f - 1.0f)); /* between 0.5 and 2? */
        afk_core.world->alterDetail(detailFactor);

        afk_core.lastCalibration = startOfFrameTime;
        afk_core.calibrationError = 0.0f;
        afk_core.graphicsDelaysSinceLastCalibration = 0;
    }
#endif

    /* Always update the controls: I think that things ought
     * to feel smoother if they continue registering movement
     * changes during glitches (worth a try, anyway).
     * The compute phase gets its own copies of the camera and
     * protagonist, so I'm not about to mess them about.
     */
    float frameInterval;
    if (afk_core.detailAdjuster->getFrameInterval(frameInterval))
    {
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_RIGHT))
            afk_core.velocity.v[0] += afk_core.config->thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_LEFT))
            afk_core.velocity.v[0] -= afk_core.config->thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_UP))
            afk_core.velocity.v[1] += afk_core.config->thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_DOWN))
            afk_core.velocity.v[1] -= afk_core.config->thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_FORWARD))
            afk_core.velocity.v[2] += afk_core.config->thrustButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_BACKWARD))
            afk_core.velocity.v[2] -= afk_core.config->thrustButtonSensitivity * frameInterval;
        
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_PITCH_UP))
            afk_core.axisDisplacement.v[0] -= afk_core.config->rotateButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_PITCH_DOWN))
            afk_core.axisDisplacement.v[0] += afk_core.config->rotateButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_YAW_RIGHT))
            afk_core.axisDisplacement.v[1] -= afk_core.config->rotateButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_YAW_LEFT))
            afk_core.axisDisplacement.v[1] += afk_core.config->rotateButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_ROLL_RIGHT))
            afk_core.axisDisplacement.v[2] -= afk_core.config->rotateButtonSensitivity * frameInterval;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_ROLL_LEFT))
            afk_core.axisDisplacement.v[2] += afk_core.config->rotateButtonSensitivity * frameInterval;
        
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
#if 0
    afk_clock::time_point waitTime = afk_clock::now();
    afk_duration_mfl frameTimeTakenSoFar = std::chrono::duration_cast<afk_duration_mfl>(
        waitTime - afk_core.detailAdjuster->getStartOfFrameTime());
    float frameTimeLeft = FRAME_REFRESH_TIME_MILLIS - frameTimeTakenSoFar.count();
    std::future_status status = afk_core.computingUpdate.wait_for(afk_duration_mfl(frameTimeLeft));
#endif
    std::future_status status = afk_core.computingUpdate.wait_for(afk_core.detailAdjuster->getComputeWaitTime());

    switch (status)
    {
    case std::future_status::ready:
        {
#if 0
            /* Work out how much time there is left on the clock */
            afk_clock::time_point afterWaitTime = afk_clock::now();
            afk_duration_mfl frameTimeTaken = std::chrono::duration_cast<afk_duration_mfl>(
                afterWaitTime - startOfFrameTime);
            float timeLeftAfterFinish = FRAME_REFRESH_TIME_MILLIS - frameTimeTaken.count();
            afk_core.calibrationError -= timeLeftAfterFinish;
            afk_core.lastFrameTime = afterWaitTime;
#endif
            afk_core.detailAdjuster->computeFinished();

            /* Flip the buffers and bump the computing frame */
            afk_core.window->swapBuffers();
            cl_context ctxt;
            cl_command_queue q;
            afk_core.computer->lock(ctxt, q);
            afk_core.renderingFrame = afk_core.computingFrame;
            afk_core.computingFrame.increment();
            afk_core.world->flipRenderQueues(afk_core.computingFrame);
            afk_core.computer->unlock();
            afk_core.computingUpdateDelayed = false;

#if FRAME_NUMBER_DEBUG || AFK_SHAPE_ENUM_DEBUG
            AFK_DEBUG_PRINTL("Now computing frame " << afk_core.computingFrame)
#endif
        
            break;
        }

    case std::future_status::timeout:
        {
#if 0
            /* Add the required amount of time to the calibration error */
            afk_core.calibrationError += frameTimeLeft;
#endif

            /* Flag this update as delayed. */
            afk_core.computingUpdateDelayed = true;
            //++afk_core.computeDelaysSinceLastCheckpoint;

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
        glDeleteBuffers(bufs.size(), &bufs[0]);
    bufs.clear();
}

/* The definition of AFK_Core itself. */

AFK_Core::AFK_Core():
    computingUpdateDelayed(false),
    detailAdjuster(nullptr),
    glGarbageBufs(1000),
    config(nullptr),
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
    if (window) delete window; /* Should close GL contexts */
    if (config) delete config;

    std::cout << "AFK: Core destroyed" << std::endl;
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
    std::cout << "AFK: Using clock with apparent tick interval: " << tickInterval.count() << " millis" << std::endl;
    assert(tickInterval.count() < 0.1f);

    config = new AFK_Config(argcp, argv);

    rng = new AFK_Boost_Taus88_RNG();
    rng->seed(config->masterSeed);

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
    window = new AFK_WindowGlx(config->windowWidth, config->windowHeight, config->vsync);
}

void AFK_Core::loop(void)
{
    /* Shader setup. */
    afk_loadShaders(config);

    computingFrame.increment();

    /* World setup. */
    computer = new AFK_Computer(config);
    computer->loadPrograms(config);

#if JIGSAW_TEST
    afk_testJigsaw(computer, config);
#endif

    /* I'll want this when I start running. */
    detailAdjuster = new AFK_DetailAdjuster(config);

    /* Now that I've set that stuff up, find out how much memory
     * I have to play with ...
     */
    unsigned int clGlMaxAllocSize = computer->getFirstDeviceProps().maxMemAllocSize;
    std::cout << "AFK: Using GPU with " << std::dec << clGlMaxAllocSize << " bytes available to cl_gl";
    std::cout << " (" << clGlMaxAllocSize / (1024 * 1024) << "MB) global memory" << std::endl;

    /* Initialise the starting objects. */
    float worldMaxDistance = config->zFar / 2.0f;

    cl_context ctxt;
    cl_command_queue q;
    computer->lock(ctxt, q);
    world = new AFK_World(
        config,
        computer,
        threadAlloc,
        worldMaxDistance,   /* maxDistance -- zFar must be a lot bigger or things will vanish */
        clGlMaxAllocSize / 4,
        clGlMaxAllocSize / 4,
        clGlMaxAllocSize / 4,
        ctxt,
        rng);
    computer->unlock();

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

        std::cout << "AFK: Checkpoint" << std::endl;
        std::cout << "AFK: Since last checkpoint: " << std::dec << renderingFrame - frameAtLastCheckpoint << " frames";
        float fps = (float)(renderingFrame - frameAtLastCheckpoint) * 1000.0f / sinceLastCheckpoint.count();
        std::cout << " (" << fps << " frames/second)" << std::endl;

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
            world->printCacheStats(std::cout, "Cache");
            world->printJigsawStats(std::cout, "Jigsaw");
        }

        std::cout << occasionalPrints.str();

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

