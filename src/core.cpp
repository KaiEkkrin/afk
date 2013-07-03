/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"
#include "computer.hpp"
#include "core.hpp"
#include "def.hpp"
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "rng/boost_taus88.hpp"


/* Static, context-less functions needed to drive GLUT, etc. */
static void afk_idle(void)
{
    ++afk_core.frameCounter;
    afk_core.printOccasionals(false);

    /* TODO For now I'm calculating the intended contents of the next frame in
     * series with drawing it.  In future, I want to move this so that it's
     * calculated in a separate thread while drawing the previous frame.
     */

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
    afk_core.axisDisplacement = Vec3<float>(0.0f, 0.0f, 0.0f);

    /* Manage the other objects.
     * TODO A call-through to AI stuff probably goes here?
     */

    /* Update the landscape, deciding which bits of it I'm going
     * to draw.
     * TODO Which dynamic objects to draw (except for the protagonist ;) )
     * should be tied into which cells get selected here.
     */
    afk_core.landscape->updateLandMap();

    afk_display();
}


/* The definition of AFK_Core itself. */

AFK_Core::AFK_Core()
{
    config          = NULL;
    computer        = NULL;
    rng             = NULL;
    camera          = NULL;
    landscape       = NULL;
    protagonist     = NULL;
    frameCounter    = 0;
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
    
    /* These functions from the `display' module. */
    glutDisplayFunc(afk_display);
    glutReshapeFunc(afk_reshape);

    /* These functions from the `event' module. */
    glutKeyboardFunc(afk_keyboard);
    glutKeyboardUpFunc(afk_keyboardUp);
    glutMouseFunc(afk_mouse);
    glutMotionFunc(afk_motion);
    glutPassiveMotionFunc(afk_motion);
    glutEntryFunc(afk_entry);

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
    velocity            = Vec3<float>(0.0f, 0.0f, 0.0f);
    axisDisplacement    = Vec3<float>(0.0f, 0.0f, 0.0f);
    controlsEnabled     = 0uLL;

    /* TODO Make the viewpoint configurable?  Right now I have a
     * fixed 3rd person view here.
     */
    camera = new AFK_Camera(Vec3<float>(0.0f, -1.5f, 3.0f));

    /* Set up the sun.  (TODO: Make configurable?  Randomly
     * generated?  W/e :) )
     */
    sun.colour[0] = 0.8f;       sun.colour[1] = 0.8f;       sun.colour[2] = 0.6f;

    Vec3<float> sunDirection = Vec3<float>(0.0f, -1.0f, 1.0f).normalise();
    sun.direction[0] = sunDirection.v[0];
    sun.direction[1] = sunDirection.v[1];
    sun.direction[2] = sunDirection.v[2];

    sun.ambient = 0.2f;
    sun.diffuse = 1.0f;
}

void AFK_Core::loop(void)
{
    /* Shader setup. */
    afk_loadShaders(config->shadersDir);

    /* Initialise the starting objects. */
    float landscapeMaxDistance = config->zFar / 2.0f;

    landscape = new AFK_Landscape( /* TODO tweak this initialisation...  extensively, and make it configurable */
        1000000,                /* cacheSize */
        landscapeMaxDistance,   /* maxDistance -- zFar must be a lot bigger or things will vanish */
        2,                      /* subdivisionFactor */
        256                     /* detailPitch.  TODO Currently this number is being interpreted as much smaller than spec'd */
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
    Vec3<float> startingPosition(0.0f, 16.0f, 0.0f);
    protagonist->object.displace(startingPosition);
    camera->displace(startingPosition);

    /* Pull the pointer into the middle before I begin so that
     * I don't start with a massive mouse axis movement right
     * away and confuse the user. */
    glutWarpPointer(windowWidth / 2, windowHeight / 2);

    glutMainLoop();
}

void AFK_Core::occasionallyPrint(const std::string& message)
{
    occasionalPrints << "AFK Frame " << frameCounter << ": " << message << std::endl;
}

void AFK_Core::printOccasionals(bool definitely)
{
    if (definitely || (frameCounter % 60) == 0)
        std::cout << occasionalPrints.str();

    occasionalPrints.str(std::string());
}

