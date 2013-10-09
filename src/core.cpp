/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include <boost/thread/future.hpp>

#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"
#include "test_jigsaw.hpp"
#include "window_glx.hpp"


#define FRAME_NUMBER_DEBUG 0


/* With a little less for wiggle room. */
#define FRAME_REFRESH_TIME 15500


#define CL_TEST 0
#define JIGSAW_TEST 0


#define CALIBRATION_INTERVAL_MICROS (afk_core.config->targetFrameTimeMicros * afk_core.config->framesPerCalibration)

void afk_displayInit(void)
{
}

void afk_idle(void)
{
    boost::posix_time::ptime startOfFrameTime = boost::posix_time::microsec_clock::local_time();

    /* If we just took less than FRAME_REFRESH_TIME for the entire
     * cycle, we're showing too little detail if we're not on a
     * Vsync system.
     */
    unsigned int wholeFrameTime = (startOfFrameTime - afk_core.startOfFrameTime).total_microseconds();
    if (!afk_core.config->vsync && wholeFrameTime < FRAME_REFRESH_TIME)
    {
        afk_core.calibrationError -= (FRAME_REFRESH_TIME - wholeFrameTime);
    }
    else
    {
        /* Work out what the graphics delay was. */
        unsigned int bufferFlipTime = (startOfFrameTime - afk_core.lastFrameTime).total_microseconds();

        /* If it's been dropping frames, there will be more than a whole
         * number of `targetFrameTimeMicros' here.
         */
        unsigned int framesDropped = bufferFlipTime / afk_core.config->targetFrameTimeMicros;
        afk_core.graphicsDelaysSinceLastCheckpoint += framesDropped;
        afk_core.graphicsDelaysSinceLastCalibration += framesDropped;
    
        /* I'm going to tolerate one random graphics delay per calibration
         * point.  Glitches happen.
         */
        if (afk_core.graphicsDelaysSinceLastCalibration > 1)
        {
            afk_core.calibrationError += afk_core.config->targetFrameTimeMicros * framesDropped;
        }
    }

    afk_core.startOfFrameTime = startOfFrameTime;

    afk_core.checkpoint(startOfFrameTime, false);

    /* Check whether I need to recalibrate. */
    /* TODO Make the time between calibrations configurable */
    boost::posix_time::time_duration sinceLastCalibration = startOfFrameTime - afk_core.lastCalibration;
    if (sinceLastCalibration.total_microseconds() > CALIBRATION_INTERVAL_MICROS)
    {
        /* Work out an error factor, and apply it to the LoD */
        float normalisedError = (float)afk_core.calibrationError / CALIBRATION_INTERVAL_MICROS; /* between -1 and 1? */

        /* Cap the normalised error: occasionally the graphics can
         * hang for ages and produce a spurious value which would
         * otherwise result in a negative detail factor, making us
         * crash
         */
        normalisedError = 0.5f * std::max(std::min(normalisedError, 1.0f), -1.0f);

        float detailFactor = -(1.0f / (normalisedError / 2.0f - 1.0f)); /* between 0.5 and 2? */

        /* TODO If I put a detail nudge in here, the render quality
         * when moving quickly is greatly improved right now, but we
         * hitch horribly at low speeds.
         * I don't really understand what's going on, although detail
         * nudges are bad :P
         */
        //if (detailFactor > 1.0f) detailFactor -= 0.02f;

        //std::cout << "calibration error " << std::dec << afk_core.calibrationError << ": applying detail factor " << detailFactor << std::endl;
        afk_core.world->alterDetail(detailFactor);

        afk_core.lastCalibration = startOfFrameTime;
        afk_core.calibrationError = 0;
        afk_core.graphicsDelaysSinceLastCalibration = 0;
    }

    if (!afk_core.computingUpdateDelayed)
    {
        /* Apply the current controls */
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_RIGHT))
            afk_core.velocity.v[0] += afk_core.config->thrustButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_LEFT))
            afk_core.velocity.v[0] -= afk_core.config->thrustButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_UP))
            afk_core.velocity.v[1] += afk_core.config->thrustButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_DOWN))
            afk_core.velocity.v[1] -= afk_core.config->thrustButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_FORWARD))
            afk_core.velocity.v[2] += afk_core.config->thrustButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_THRUST_BACKWARD))
            afk_core.velocity.v[2] -= afk_core.config->thrustButtonSensitivity;

        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_PITCH_UP))
            afk_core.axisDisplacement.v[0] -= afk_core.config->rotateButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_PITCH_DOWN))
            afk_core.axisDisplacement.v[0] += afk_core.config->rotateButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_YAW_RIGHT))
            afk_core.axisDisplacement.v[1] -= afk_core.config->rotateButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_YAW_LEFT))
            afk_core.axisDisplacement.v[1] += afk_core.config->rotateButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_ROLL_RIGHT))
            afk_core.axisDisplacement.v[2] -= afk_core.config->rotateButtonSensitivity;
        if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_ROLL_LEFT))
            afk_core.axisDisplacement.v[2] += afk_core.config->rotateButtonSensitivity;

        afk_core.camera->driveAndUpdateProjection(afk_core.velocity, afk_core.axisDisplacement);

        /* Protagonist follow camera.  (To make it look more
         * natural, at some point I want a stretchy leash)
         */
        afk_core.protagonist->object.drive(afk_core.velocity, afk_core.axisDisplacement);

        /* Swallow the axis displacement after it's been applied
         * so that I don't get a strange mouse acceleration effect
         */
        afk_core.axisDisplacement = afk_vec3<float>(0.0f, 0.0f, 0.0f);

        /* Manage the other objects.
         * TODO A call-through to AI stuff probably goes here?
         */

        /* Update the world, deciding which bits of it I'm going
         * to draw.
         */
        afk_core.computingUpdate = afk_core.world->updateWorld();

        /* Meanwhile, draw the previous frame */
        afk_display();
    }

    /* Clean up anything that's gotten queued into the garbage queue */
    /* TODO take this out, enable the workers to delete their own
     * GL garbage
     */
    afk_core.deleteGlGarbageBufs();

    /* Wait until it's about time to display the next frame. */
    boost::posix_time::ptime waitTime = boost::posix_time::microsec_clock::local_time();
    int frameTimeLeft = FRAME_REFRESH_TIME - (waitTime - startOfFrameTime).total_microseconds();
    boost::chrono::microseconds chFrameTimeLeft(frameTimeLeft);
    boost::future_status status = afk_core.computingUpdate.wait_for(chFrameTimeLeft);

    switch (status)
    {
    case boost::future_status::ready:
        {
            /* Work out how much time there is left on the clock */
            boost::posix_time::ptime now = boost::posix_time::microsec_clock::local_time();
            int timeToFinish = (now - waitTime).total_microseconds();
            int timeLeftAfterFinish = frameTimeLeft - timeToFinish;
            afk_core.calibrationError -= timeLeftAfterFinish;
            afk_core.lastFrameTime = now;

            /* Flip the buffers and bump the computing frame */
            afk_core.window->swapBuffers();
            cl_context ctxt;
            cl_command_queue q;
            afk_core.computer->lock(ctxt, q);
            afk_core.renderingFrame = afk_core.computingFrame;
            afk_core.computingFrame.increment();
            afk_core.world->flipRenderQueues(ctxt, afk_core.computingFrame);
            afk_core.computer->unlock();
            afk_core.computingUpdateDelayed = false;

#if FRAME_NUMBER_DEBUG
            AFK_DEBUG_PRINTL("Now computing frame " << afk_core.computingFrame)
#endif
        
            break;
        }

    case boost::future_status::timeout:
        {
            /* Add the required amount of time to the calibration error */
            afk_core.calibrationError += frameTimeLeft;

            /* Flag this update as delayed. */
            afk_core.computingUpdateDelayed = true;
            ++afk_core.computeDelaysSinceLastCheckpoint;

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
    computeDelaysSinceLastCheckpoint(0),
    graphicsDelaysSinceLastCheckpoint(0),
    graphicsDelaysSinceLastCalibration(0),
    calibrationError(0),
    glGarbageBufs(1000),
    config(NULL),
    computer(NULL),
    window(NULL),
    rng(NULL),
    camera(NULL),
    world(NULL),
    protagonist(NULL)
{
}

AFK_Core::~AFK_Core()
{
    /* I need to be a bit careful about the order I do
     * this in ...
     */
    if (window) afk_core.window->closeWindow();
    if (world) delete world; /* Should stop the threads */
    if (rng) delete rng;
    if (camera) delete camera;
    if (protagonist) delete protagonist;

    if (computer) delete computer; /* Should close CL contexts */
    if (window) delete window; /* Should close GL contexts */
    if (config) delete config;

    std::cout << "AFK: Core destroyed" << std::endl;
}

void AFK_Core::configure(int *argcp, char **argv)
{
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
    camera = new AFK_Camera(afk_vec3<float>(0.0f, -1.5f, 3.0f));

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
    afk_loadShaders(config->shadersDir);

    computingFrame.increment();

    /* World setup. */
    computer = new AFK_Computer();
    computer->loadPrograms(config);

#if JIGSAW_TEST
    afk_testJigsaw(computer, config);
#endif

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
        worldMaxDistance,   /* maxDistance -- zFar must be a lot bigger or things will vanish */
        clGlMaxAllocSize / 4,
        clGlMaxAllocSize / 4,
        clGlMaxAllocSize / 4,
        ctxt,
        rng);
    computer->unlock();
    protagonist = new AFK_DisplayedProtagonist();

    /* Make sure that camera is configured to match the window. */
    camera->setWindowDimensions(
        window->getWindowWidth(), window->getWindowHeight());

    /* TODO: Move the camera to somewhere above the landscape to
     * start with.
     * (A bit unclear how to best do this ... )
     * For now, I'll just start them nice and high to reduce the
     * chance of spawning under the landscape (very annoying)
     */
    Vec3<float> startingMovement = afk_vec3<float>(0.0f, 8192.0f, 0.0f);
    Vec3<float> startingRotation = afk_vec3<float>(0.0f, 0.0f, 0.0f);
    protagonist->object.drive(startingMovement, startingRotation);
    camera->driveAndUpdateProjection(startingMovement, startingRotation);

    /* First checkpoint */
    startOfFrameTime = lastFrameTime = lastCheckpoint = lastCalibration =
        boost::posix_time::microsec_clock::local_time();
    computeDelaysSinceLastCheckpoint =
        graphicsDelaysSinceLastCheckpoint =
        graphicsDelaysSinceLastCalibration = 0;

    /* Branch the main thread into the window event handling loop. */
    window->loopOnEvents(
        afk_idle,
        afk_keyboardUp,
        afk_keyboard,
        afk_mouseUp,
        afk_mouse,
        afk_motion);
}

const boost::posix_time::ptime& AFK_Core::getStartOfFrameTime(void) const
{
    return startOfFrameTime;
}

void AFK_Core::occasionallyPrint(const std::string& message)
{
    occasionalPrints << "AFK Frame " << renderingFrame.get() << ": " << message << std::endl;
}

void AFK_Core::checkpoint(boost::posix_time::ptime& now, bool definitely)
{
    boost::posix_time::time_duration sinceLastCheckpoint = now - lastCheckpoint;

    if ((definitely || sinceLastCheckpoint.total_seconds() > 0) &&
        !(frameAtLastCheckpoint == renderingFrame))
    {
        lastCheckpoint = now;

        std::cout << "AFK: Checkpoint at " << now << std::endl;
        std::cout << "AFK: Since last checkpoint: " << std::dec << renderingFrame - frameAtLastCheckpoint << " frames";
        float fps = (float)(renderingFrame - frameAtLastCheckpoint) * 1000.0f / (float)sinceLastCheckpoint.total_milliseconds();
        std::cout << " (" << fps << " frames/second)";
        std::cout << " (" << computeDelaysSinceLastCheckpoint * 100 / (renderingFrame - frameAtLastCheckpoint) << "\% compute delay, ";
        std::cout << graphicsDelaysSinceLastCheckpoint * 100 / (renderingFrame - frameAtLastCheckpoint) << "\% graphics delay)" << std::endl;

        frameAtLastCheckpoint = renderingFrame;
        computeDelaysSinceLastCheckpoint = 0;
        graphicsDelaysSinceLastCheckpoint = 0;

        if (world) world->checkpoint(sinceLastCheckpoint);

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
    }

    occasionalPrints.str(std::string());
}

void AFK_Core::glBuffersForDeletion(GLuint *bufs, size_t bufsSize)
{
    for (size_t i = 0; i < bufsSize; ++i)
        glGarbageBufs.push(bufs[i]);
}

