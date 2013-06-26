/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "config.hpp"
#include "core.hpp"
#include "event.hpp"

static void displaceAxis(enum AFK_Control_Axes axis, float displacement)
{
    if (AFK_TEST_BIT(afk_core.config->axisInversionMap, axis))
        displacement = -displacement;

    switch (axis)
    {
    case CTRL_AXIS_PITCH:
        afk_core.axisDisplacement.v[0] += displacement;
        break;

    case CTRL_AXIS_YAW:
        afk_core.axisDisplacement.v[1] += displacement;
        break;

    case CTRL_AXIS_ROLL:
        afk_core.axisDisplacement.v[2] += displacement;
        break;

    default:
        break;
    }
}

static void enableControl(enum AFK_Controls control)
{
    if (AFK_IS_TOGGLE(control))
        AFK_TEST_BIT(afk_core.controlsEnabled, control) ?
            AFK_CLEAR_BIT(afk_core.controlsEnabled, control) :
            AFK_SET_BIT(afk_core.controlsEnabled, control);
    else
        AFK_SET_BIT(afk_core.controlsEnabled, control);

    if (control == CTRL_MOUSE_CAPTURE)
        glutWarpPointer(afk_core.camera.windowWidth / 2, afk_core.camera.windowHeight / 2);
}

static void disableControl(enum AFK_Controls control)
{
    if (!AFK_IS_TOGGLE(control)) AFK_CLEAR_BIT(afk_core.controlsEnabled, control);
    if (control == CTRL_MOUSE_CAPTURE)
    {
        /* Reset the relevant axes. */
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_X], 0.0f);
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_Y], 0.0f);
    }
}

void afk_keyboard(unsigned char key, int x, int y)
{
    enableControl(afk_core.config->keyboardMapping[key]);
}

void afk_keyboardUp(unsigned char key, int x, int y)
{
    disableControl(afk_core.config->keyboardMapping[key]);
}

void afk_mouse(int button, int state, int x, int y)
{
    switch (state)
    {
    case GLUT_DOWN:
        enableControl(afk_core.config->mouseMapping[button]);
        break;

    case GLUT_UP:
        disableControl(afk_core.config->mouseMapping[button]);
        break;
    }
}

void afk_motion(int x, int y)
{
    int x_midpoint = afk_core.camera.windowWidth / 2;
    int y_midpoint = afk_core.camera.windowHeight / 2;

    if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_MOUSE_CAPTURE) &&
        (x != x_midpoint || y != y_midpoint))
    {
        int x_displacement = x - x_midpoint;
        int y_displacement = y - y_midpoint;

        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_X],
            afk_core.config->mouseAxisSensitivity * (float)x_displacement);
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_Y],
            afk_core.config->mouseAxisSensitivity * (float)y_displacement);

        /* Pull the pointer back to the midpoint; my next
         * displacement will be relative to it.
         */
        glutWarpPointer(x_midpoint, y_midpoint);
    }
}

void afk_entry(int state)
{
    /* TODO Do I actually need to do anything here? */
    //if (state == GLUT_LEFT) disableControl(CTRL_MOUSE_CAPTURE);
}

