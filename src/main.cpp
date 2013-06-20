/* AFK (c) Alex Holloway 2013 */

/* TODO Convert this stuff to C++11.  It's genuinely better and I'm already
 * noticing places where I would benefit from using it instead:
 * - use unique_ptr instead of auto_ptr
 * - use unordered_map instead of map
 * - use { ... } initialisation assignments instead of cumbersome messes
 */

#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GL/glut.h>

#include "display.h"
#include "event.h"
#include "shader.h"
#include "state.h"

/* This is the AFK global state declared in state.h */
struct AFK_State afk_state;

/* AFK main and related stuff. */

static void afk_idle(void)
{
    /* TODO For now I'm calculating the intended contents of the next frame in
     * series with drawing it.  In future, I want to move this so that it's
     * calculated in a separate thread while drawing the previous frame.
     */

    /* Apply the current controls */
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_THRUST_RIGHT))
        afk_state.velocity.v[0] += afk_state.config->thrustButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_THRUST_LEFT))
        afk_state.velocity.v[0] -= afk_state.config->thrustButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_THRUST_UP))
        afk_state.velocity.v[1] += afk_state.config->thrustButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_THRUST_DOWN))
        afk_state.velocity.v[1] -= afk_state.config->thrustButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_THRUST_FORWARD))
        afk_state.velocity.v[2] += afk_state.config->thrustButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_THRUST_BACKWARD))
        afk_state.velocity.v[2] -= afk_state.config->thrustButtonSensitivity;

    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_PITCH_UP))
        afk_state.axisDisplacement.v[0] += afk_state.config->rotateButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_PITCH_DOWN))
        afk_state.axisDisplacement.v[0] -= afk_state.config->rotateButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_YAW_RIGHT))
        afk_state.axisDisplacement.v[1] += afk_state.config->rotateButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_YAW_LEFT))
        afk_state.axisDisplacement.v[1] -= afk_state.config->rotateButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_ROLL_RIGHT))
        afk_state.axisDisplacement.v[2] += afk_state.config->rotateButtonSensitivity;
    if (AFK_TEST_BIT(afk_state.controlsEnabled, CTRL_ROLL_LEFT))
        afk_state.axisDisplacement.v[2] -= afk_state.config->rotateButtonSensitivity;
    
    afk_state.camera.drive();

    afk_state.axisDisplacement = Vec3<float>(0.0f, 0.0f, 0.0f);

    afk_nextFrame();
    afk_display();
}

int main(int argc, char **argv)
{
    GLenum res;

    glutInit(&argc, argv); // TODO check what exactly this does
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

    glutIdleFunc(afk_idle);

    /* Local configuration. */
    afk_state.camera.setWindowDimensions(glutGet(GLUT_WINDOW_HEIGHT), glutGet(GLUT_WINDOW_WIDTH));
    afk_state.config            = new AFK_Config(&argc, argv);
    afk_state.velocity          = Vec3<float>(0.0f, 0.0f, 0.0f);
    afk_state.axisDisplacement  = Vec3<float>(0.0f, 0.0f, 0.0f);
    afk_state.controlsEnabled   = 0uLL;

    /* Extension detection. */
    res = glewInit();
    if (res != GLEW_OK)
    {
        fprintf(stderr, "Failed to initialise GLEW: %s\n", glewGetErrorString(res));
        exit(1);
    }

    /* Shader setup. */
    if (!afk_loadShaders(afk_state.config->shadersDir)) exit(1);

    /* Display setup. */
    afk_displayInit();

    /* Pull the pointer into the middle before I begin so that
     * I don't start with a massive mouse axis movement right
     * away and confuse the user. */
    glutWarpPointer(afk_state.camera.windowWidth / 2, afk_state.camera.windowHeight / 2);

    glutMainLoop();
    return 0;
}

