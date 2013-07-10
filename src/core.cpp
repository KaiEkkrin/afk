/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <boost/thread/future.hpp>

#include "computer.hpp"
#include "core.hpp"
#include "def.hpp"
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"


/* TODO Maybe move these statics that are governing the calibrator
 * into configuration?
 * Oh God, I've got fixed bits of configuration *everywhere* now :(
 */
#define EXPECTED_FRAME_TIME_MICROS 15500
#define CALIBRATION_INTERVAL_MICROS (EXPECTED_FRAME_TIME_MICROS * 16)
#define SKIPPED_FRAME_TOLERANCE 4.0f /* okay to skip one frame in this many before reducing LoD */
#define FRAMES_PER_CALIBRATION_INTERVAL ((float)CALIBRATION_INTERVAL_MICROS / (float)EXPECTED_FRAME_TIME_MICROS)

/* Static, context-less functions needed to drive GLUT, etc. */
void afk_idle(void)
{
    /* Time how long it takes me to do setup */
    boost::posix_time::ptime startOfFrameTime = boost::posix_time::microsec_clock::local_time();

    afk_core.checkpoint(startOfFrameTime, false);

    /* Check whether I need to recalibrate. */
    /* TODO Make the time between calibrations configurable */
    boost::posix_time::time_duration sinceLastCalibration = startOfFrameTime - afk_core.lastCalibration;
    if (sinceLastCalibration.total_microseconds() > CALIBRATION_INTERVAL_MICROS)
    {
        if (afk_core.delaysSinceLastCalibration == 0)
            afk_core.landscape->increaseDetail();
        else if ((FRAMES_PER_CALIBRATION_INTERVAL / afk_core.delaysSinceLastCalibration) < SKIPPED_FRAME_TOLERANCE)
            afk_core.landscape->decreaseDetail();

        afk_core.lastCalibration = startOfFrameTime;
        afk_core.delaysSinceLastCalibration = 0;
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

        /* Update the landscape, deciding which bits of it I'm going
         * to draw.
         */
        afk_core.computingUpdate = afk_core.landscape->updateLandMap();

        /* Meanwhile, draw the previous frame */
        afk_display();
    }

    /* Clean up anything that's gotten queued into the garbage queue */
    afk_core.deleteGlGarbageBufs();

    /* Wait until it's about time to display the next frame.
     * I want to wait for 16 milliseconds minus a little bit (let's
     * call it 15.5 milliseconds, or 15500 microseconds), minus the
     * amount of time I've already spent.
     * TODO It would be better if the toolkit could do this for us:
     * this method might be deleterious to input.  GLUT, eh?
     */
    boost::posix_time::ptime waitTime = boost::posix_time::microsec_clock::local_time();
    int frameTimeLeft = EXPECTED_FRAME_TIME_MICROS - (waitTime - startOfFrameTime).total_microseconds();
    boost::this_thread::sleep_for(boost::chrono::microseconds(frameTimeLeft));
    if (afk_core.computingUpdate.is_ready())
    {
        /* Great.  Flip the buffers and bump the computing frame */
        glutSwapBuffers();
        afk_core.landscape->flipRenderQueues();
        afk_core.computingUpdateDelayed = false;
        afk_core.computingFrame = afk_core.renderingFrame;
        afk_core.renderingFrame.increment();
    }
    else
    {
        /* We were too slow.
         * TODO: This is the place where I should add logic that
         * considers adjusting the detail pitch in order to keep 
         * up.
         */
        afk_core.computingUpdateDelayed = true;
        ++afk_core.delaysSinceLastCheckpoint;
        ++afk_core.delaysSinceLastCalibration;
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
    delaysSinceLastCalibration(0),
    delaysSinceLastCheckpoint(0),
    glGarbageBufs(1000),
    config(NULL),
    computer(NULL),
    rng(NULL),
    camera(NULL),
    landscape(NULL),
    protagonist(NULL)
{
}

AFK_Core::~AFK_Core()
{
    if (config) delete config;
    if (computer) delete computer;
    if (rng) delete rng;
    if (camera) delete camera;
    if (landscape) delete landscape;
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

void AFK_Core::initCompute(void)
{
    computer = new AFK_Computer();
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
    sun.diffuse = 0.6f;
}

void AFK_Core::loop(void)
{
    /* Shader setup. */
    afk_loadShaders(config->shadersDir);

    /* Initialise the starting objects. */
    float landscapeMaxDistance = config->zFar / 2.0f;

    landscape = new AFK_Landscape( /* TODO tweak this initialisation...  extensively, and make it configurable */
        landscapeMaxDistance,   /* maxDistance -- zFar must be a lot bigger or things will vanish */
        2,                      /* subdivisionFactor */
        8                       /* pointSubdivisionFactor */
        );
    protagonist = new AFK_DisplayedProtagonist();

    /* Make sure that camera is configured to match the window. */
    int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
    int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
    camera->setWindowDimensions(windowWidth, windowHeight);

    /* Move the camera and protagonist to somewhere close to the landscape
     * so they can see
     * TODO: Better, evaluate the landscape in the protagonist's cell and
     * move the protagonist to just above it!
     */
    Vec3<float> startingPosition = afk_vec3<float>(0.0f, 16.0f, 0.0f);
    protagonist->object.displace(startingPosition);
    camera->displace(startingPosition);

    /* Pull the pointer into the middle before I begin so that
     * I don't start with a massive mouse axis movement right
     * away and confuse the user. */
    glutWarpPointer(windowWidth / 2, windowHeight / 2);

    /* First checkpoint */
    lastCheckpoint = boost::posix_time::microsec_clock::local_time();
    lastCalibration = boost::posix_time::microsec_clock::local_time();
    delaysSinceLastCheckpoint = 0;

    glutMainLoop();
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
        std::cout << " (" << delaysSinceLastCheckpoint * 100 / (renderingFrame - frameAtLastCheckpoint) << "\% delayed)" << std::endl;

        frameAtLastCheckpoint = renderingFrame;
        delaysSinceLastCheckpoint = 0;

        if (landscape) landscape->checkpoint(sinceLastCheckpoint);

        /* BODGE. This can cause a SIGSEGV when we do the final print upon
         * catching an exception, it looks like the destructors are
         * being called in a strange order
         */
        if (!definitely)
        {
            std::ostringstream ss;
            afk_core.landscape->cache.printStats(ss, "Cache");
            std::cout << ss.str();
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


