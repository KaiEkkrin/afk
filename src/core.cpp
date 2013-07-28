/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <cmath>

#include <boost/thread/future.hpp>

#include "compute_test_long.hpp"
#include "core.hpp"
#include "debug.hpp"
#include "def.hpp"
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"


#define FRAME_NUMBER_DEBUG 0


/* With a little less for wiggle room. */
#define FRAME_REFRESH_TIME 15500


/* TODO Maybe move these statics that are governing the calibrator
 * into configuration?
 * Oh God, I've got fixed bits of configuration *everywhere* now :(
 */
#define CALIBRATION_INTERVAL_MICROS (afk_core.config->targetFrameTimeMicros * afk_core.config->framesPerCalibration)

/* Static, context-less functions needed to drive GLUT, etc. */
void afk_idle(void)
{
    boost::posix_time::ptime startOfFrameTime = boost::posix_time::microsec_clock::local_time();

    /* If we just took less than FRAME_REFRESH_TIME for the entire
     * cycle, we're showing too little detail if we're not on a
     * Vsync system.
     * TODO I could really do with being able to detect that, or
     * being in control of it, or something.  Maybe at some point
     * I should try monitoring and understanding all these delays
     * more properly.
     */
    unsigned int wholeFrameTime = (startOfFrameTime - afk_core.startOfFrameTime).total_microseconds();
    if (!afk_core.config->assumeVsync && wholeFrameTime < FRAME_REFRESH_TIME)
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
        normalisedError = std::max(std::min(normalisedError, 1.0f), -1.0f);

        float detailFactor = -(1.0f / (normalisedError / 2.0f - 1.0f)); /* between 0.5 and 2? */
        //std::cout << "calibration error " << std::dec << afk_core.calibrationError << ": applying detail factor " << detailFactor << std::endl;
        afk_core.world->alterDetail(detailFactor);

        afk_core.lastCalibration = startOfFrameTime;
        afk_core.calibrationError = 0;
        afk_core.graphicsDelaysSinceLastCalibration = 0;
    }

    if (!afk_core.computingUpdateDelayed)
    {
        /* Apply the current controls */
        /* TODO At some point, it may be possible to have concurrent access to
         * these and I'll need to guard against that.  But right now that doesn't
         * seem to happen :p
         */
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
    
        afk_core.camera->drive(afk_core.velocity, afk_core.axisDisplacement);

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
    afk_core.deleteGlGarbageBufs();

    /* Wait until it's about time to display the next frame.
     * TODO It would be better if the toolkit could do this for us:
     * this method might be deleterious to input.  GLUT, eh?
     */
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
            glutSwapBuffers();
            afk_core.world->flipRenderQueues();
            afk_core.computingUpdateDelayed = false;
            afk_core.computingFrame = afk_core.renderingFrame;
            afk_core.renderingFrame.increment();

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
    rng(NULL),
    camera(NULL),
    world(NULL),
    protagonist(NULL)
{
}

AFK_Core::~AFK_Core()
{
    if (config) delete config;
    if (computer) delete computer;
    if (rng) delete rng;
    if (camera) delete camera;
    if (world) delete world;
    if (protagonist) delete protagonist;

    std::cout << "AFK: Core destroyed" << std::endl;
}

void AFK_Core::initGraphics(int *argcp, char **argv)
{
    GLenum res;

    glutInit(argcp, argv); // TODO check what exactly this does
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);

    /* TODO Full screen by default?  Pull configuration from file / from cmdline? */
    glutCreateWindow("AFK");

    /* Work out the screen size in case I decide to switch to
     * fullscreen mode.
     * TODO: Configurable screen mode selection?  (GLUT seems
     * to make this hard.)
     */
    int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
    int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);

    {
        std::ostringstream gms;
        gms << screenWidth << "x" << screenHeight << ":" << 24;
        glutGameModeString(gms.str().c_str());
    }
    
    /* These functions from the `display' module. */
    glutDisplayFunc(afk_display);
    glutReshapeFunc(afk_reshape);

    /* These functions from the `event' module. */
    glutEntryFunc(afk_entry);
    glutKeyboardFunc(afk_keyboard);
    glutKeyboardUpFunc(afk_keyboardUp);
    glutMouseFunc(afk_mouse);
    glutMotionFunc(afk_motion);
    glutPassiveMotionFunc(afk_motion);
    glutSpecialFunc(afk_special);
    glutSpecialUpFunc(afk_specialUp);

    /* Specifies the loop function. */
    glutIdleFunc(afk_idle);

    /* Make GLUT give us control back on error or close. */
    //glutInitErrorFunc(afk_glutError);
    //glutInitWarningFunc(afk_glutWarning);
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);

    /* Extension detection. */
    res = glewInit();
    if (res != GLEW_OK) throw AFK_Exception(std::string("Failed to initialise GLEW: "), glewGetErrorString(res));
}

void AFK_Core::configure(int *argcp, char **argv)
{
    config = new AFK_Config(argcp, argv);

    rng = new AFK_Boost_Taus88_RNG();

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
     */
    sun.colour = afk_vec3<float>(1.0f, 1.0f, 1.0f);
    sun.direction = afk_vec3<float>(0.0f, -1.0f, 1.0f).normalise();
    sun.ambient = 0.2f;
    sun.diffuse = 1.0f;
}

void AFK_Core::initCompute(void)
{
    computer = new AFK_Computer(config->concurrency);
    std::string clProgramsDirStr(config->clProgramsDir);
    computer->loadPrograms(clProgramsDirStr);
}

void AFK_Core::testCompute(void)
{
    afk_testComputeLong(computer, config->concurrency);
}

void AFK_Core::loop(void)
{
    /* Shader setup. */
    afk_loadShaders(config->shadersDir);

    /* Initialise the starting objects. */
    float worldMaxDistance = config->zFar / 2.0f;

    world = new AFK_World(
        worldMaxDistance,   /* maxDistance -- zFar must be a lot bigger or things will vanish */
        config->subdivisionFactor,
        config->pointSubdivisionFactor,
        config->minCellSize,
        config->startingDetailPitch,
        config->concurrency
        );
    protagonist = new AFK_DisplayedProtagonist();

    /* Make sure that camera is configured to match the window. */
    int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
    int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
    camera->setWindowDimensions(windowWidth, windowHeight);

    /* TODO: Move the camera to somewhere above the landscape to
     * start with.
     * (A bit unclear how to best do this ... )
     */

    /* Pull the pointer into the middle before I begin so that
     * I don't start with a massive mouse axis movement right
     * away and confuse the user. */
    glutWarpPointer(windowWidth / 2, windowHeight / 2);

    /* First checkpoint */
    startOfFrameTime = lastFrameTime = lastCheckpoint = lastCalibration =
        boost::posix_time::microsec_clock::local_time();
    computeDelaysSinceLastCheckpoint =
        graphicsDelaysSinceLastCheckpoint =
        graphicsDelaysSinceLastCalibration = 0;

    renderingFrame.increment();

    glutMainLoop();
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
        if (!definitely) afk_core.world->printCacheStats(std::cout, "Cache");

        std::cout << occasionalPrints.str();
    }

    occasionalPrints.str(std::string());
}

void AFK_Core::glBuffersForDeletion(GLuint *bufs, size_t bufsSize)
{
    for (size_t i = 0; i < bufsSize; ++i)
        glGarbageBufs.push(bufs[i]);
}

