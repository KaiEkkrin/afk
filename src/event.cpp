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

    switch (control)
    {
    case CTRL_MOUSE_CAPTURE:
        if (AFK_TEST_BIT(afk_core.controlsEnabled, control))
            afk_core.window->capturePointer();
        else
            afk_core.window->letGoOfPointer();
        break;

    case CTRL_FULLSCREEN:
        if (AFK_TEST_BIT(afk_core.controlsEnabled, control))
            afk_core.window->switchToFullScreen();
        else
            afk_core.window->switchAwayFromFullScreen();
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
    enableControl(afk_core.config->keyboardMapping[key]);
}

void afk_keyboardUp(unsigned int key)
{
    disableControl(afk_core.config->keyboardMapping[key]);
}

void afk_mouse(unsigned int button)
{
    enableControl(afk_core.config->mouseMapping[button]);
}

void afk_mouseUp(unsigned int button)
{
    disableControl(afk_core.config->mouseMapping[button]);
}

void afk_motion(int x, int y)
{
    if (AFK_TEST_BIT(afk_core.controlsEnabled, CTRL_MOUSE_CAPTURE))
    {
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_X],
            afk_core.config->mouseAxisSensitivity * (float)x);
        displaceAxis(afk_core.config->mouseAxisMapping[MOUSE_AXIS_Y],
            afk_core.config->mouseAxisSensitivity * (float)y);
    }
}

