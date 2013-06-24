/* AFK (c) Alex Holloway 2013 */

#include "afk.h"

#include <iostream>

#include "core.h"
#include "def.h"
#include "display.h"
#include "event.h"
#include "exception.h"


/* Static, context-less functions needed to drive GLUT, etc. */
static void afk_idle(void)
{
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

    afk_core.camera.drive(afk_core.velocity, afk_core.axisDisplacement);

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
     * For now, I'm just going to wiggle testObject about in a
     * silly manner.
     */
    afk_core.testObject->object.adjustAttitude(AXIS_ROLL, 0.02f);
    afk_core.testObject->object.displace(Vec3<float>(0.0f, 0.03f, 0.0f));
    afk_display();
}


/* The definition of AFK_Core itself. */

AFK_Core::AFK_Core()
{
    config          = NULL;
    testObject      = NULL;
    landscapeObject = NULL;
    protagonist     = NULL;
}

AFK_Core::~AFK_Core()
{
    if (config) delete config;
    for (std::vector<AFK_DisplayedObject *>::iterator do_it = dos.begin(); do_it != dos.end(); ++do_it)
        delete *do_it;

    std::cout << "AFK: Core destroyed" << std::endl;
}

void AFK_Core::initGraphics(int *argcp, char **argv)
{
    GLenum res;

    glutInit(argcp, argv); // TODO check what exactly this does
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);

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

/* TODO.
void AFK_Core::initCompute(void)
{
}
*/

void AFK_Core::configure(int *argcp, char **argv)
{
    config = new AFK_Config(argcp, argv);

    /* Startup state of the protagonist. */
    velocity            = Vec3<float>(0.0f, 0.0f, 0.0f);
    axisDisplacement    = Vec3<float>(0.0f, 0.0f, 0.0f);
    controlsEnabled     = 0uLL;

    /* TODO Make the viewpoint configurable?  Right now I have a
     * fixed 3rd person view here.
     */
    camera.separation   = Vec3<float>(0.0f, -1.5f, 3.0f);
}

void AFK_Core::loop(void)
{
    /* Shader setup. */
    afk_loadShaders(config->shadersDir);

    /* Initialise the starting objects. */
    testObject = new AFK_DisplayedTestObject();
    landscapeObject = new AFK_DisplayedLandscapeObject();
    protagonist = new AFK_DisplayedProtagonist();

    dos.push_back(testObject);
    dos.push_back(landscapeObject);
    dos.push_back(protagonist);

    /* Make sure that camera is configured to match the window. */
    int windowWidth = glutGet(GLUT_WINDOW_WIDTH);
    int windowHeight = glutGet(GLUT_WINDOW_HEIGHT);
    camera.setWindowDimensions(windowWidth, windowHeight);

    /* Pull the pointer into the middle before I begin so that
     * I don't start with a massive mouse axis movement right
     * away and confuse the user. */
    glutWarpPointer(windowWidth / 2, windowHeight / 2);

    glutMainLoop();
}

