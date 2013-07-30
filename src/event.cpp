/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include "config.hpp"
#include "core.hpp"
#include "display.hpp"
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

#if 0
    /* Some special handling. */
    static int oldWindowWidth = -1, oldWindowHeight = -1;
#endif

    switch (control)
    {
    case CTRL_MOUSE_CAPTURE:
        if (AFK_TEST_BIT(afk_core.controlsEnabled, control))
        {
            afk_core.window->capturePointer();
        }
        else
        {
            afk_core.window->letGoOfPointer();
        }
        break;

    case CTRL_FULLSCREEN:
        /* TODO Fix this when I've figured out how to fullscreen
         * a window with Xlib.
         */
#if 0
        if (AFK_TEST_BIT(afk_core.controlsEnabled, control))
        {
            oldWindowWidth = glutGet(GLUT_WINDOW_WIDTH);
            oldWindowHeight = glutGet(GLUT_WINDOW_HEIGHT);
            
            int screenWidth = glutGet(GLUT_SCREEN_WIDTH);
            int screenHeight = glutGet(GLUT_SCREEN_HEIGHT);

            glutReshapeWindow(screenWidth, screenHeight);
            glutFullScreen();

            /* TODO When I enter game mode, all subsequent events get swallowed.
             * I think I might need to debug freeglut3 to figure out what's
             * going on there.  In the meantime, leaving this bit out.
             */
#define USE_GAME_MODE 0

#if USE_GAME_MODE
            glutEnterGameMode();
#endif
        }
        else
        {
#if USE_GAME_MODE
            glutLeaveGameMode();
#endif

            if (oldWindowWidth != -1 && oldWindowHeight != -1)
            {
                glutReshapeWindow(oldWindowWidth, oldWindowHeight);
            }
        }
#endif
        break;

    default:
        /* Nothing else to do. */
        break;

    }
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

void afk_keyboard(unsigned int key)
{
    afk_core.inputMut.lock();
    enableControl(afk_core.config->keyboardMapping[key]);
    afk_core.inputMut.unlock();
}

void afk_keyboardUp(unsigned int key)
{
    afk_core.inputMut.lock();
    disableControl(afk_core.config->keyboardMapping[key]);
    afk_core.inputMut.unlock();
}

void afk_mouse(unsigned int button)
{
    afk_core.inputMut.lock();
    enableControl(afk_core.config->mouseMapping[button]);
    afk_core.inputMut.unlock();
}

void afk_mouseUp(unsigned int button)
{
    afk_core.inputMut.lock();
    disableControl(afk_core.config->mouseMapping[button]);
    afk_core.inputMut.unlock();
}

void afk_motion(int x, int y)
{
    afk_core.inputMut.lock();
    if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_MOUSE_CAPTURE))
    {
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_X],
            afk_core.config->mouseAxisSensitivity * (float)x);
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_Y],
            afk_core.config->mouseAxisSensitivity * (float)y);
    }
    afk_core.inputMut.unlock();
}

