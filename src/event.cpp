/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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

#include "core.hpp"
#include "debug.hpp"
#include "display.hpp"
#include "event.hpp"
#include "ui/config_settings.hpp"

#define DEBUG_CONTROLS 0

static void displaceAxis(AFK_Control axis, float displacement)
{
    assert((static_cast<int>(axis)& AFK_CONTROL_AXIS_BIT) != 0);
    switch (axis)
    {
    case AFK_Control::AXIS_PITCH:
        afk_core.axisDisplacement.v[0] += (displacement * afk_core.settings.getAxisInversion(axis));
        break;

    case AFK_Control::AXIS_YAW:
        afk_core.axisDisplacement.v[1] += (displacement * afk_core.settings.getAxisInversion(axis));
        break;

    case AFK_Control::AXIS_ROLL:
        afk_core.axisDisplacement.v[2] += (displacement * afk_core.settings.getAxisInversion(axis));
        break;

    default:
        break;
    }
}

static void enableControl(AFK_Control control)
{
    if (AFK_CONTROL_IS_TOGGLE(control))
        AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, control) ?
            AFK_CLEAR_CONTROL_BIT(afk_core.controlsEnabled, control) :
            AFK_SET_CONTROL_BIT(afk_core.controlsEnabled, control);
    else
        AFK_SET_CONTROL_BIT(afk_core.controlsEnabled, control);

    switch (control)
    {
    case AFK_Control::MOUSE_CAPTURE:
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, control))
            afk_core.window->capturePointer();
        else
            afk_core.window->letGoOfPointer();
        break;

    case AFK_Control::FULLSCREEN:
        if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, control))
            afk_core.window->switchToFullScreen();
        else
            afk_core.window->switchAwayFromFullScreen();
        break;

    default:
        /* Nothing else to do. */
        break;

    }
}

static void disableControl(AFK_Control control)
{
    if (!AFK_CONTROL_IS_TOGGLE(control)) AFK_CLEAR_CONTROL_BIT(afk_core.controlsEnabled, control);
    if (control == AFK_Control::MOUSE_CAPTURE)
    {
        /* Reset the relevant axes. */
        displaceAxis(afk_core.settings.mouseAxisControls[AFK_MouseAxis::X], 0.0f);
        displaceAxis(afk_core.settings.mouseAxisControls[AFK_MouseAxis::Y], 0.0f);
    }
}

void afk_keyboard(const std::string& key)
{
#if DEBUG_CONTROLS
    AFK_DEBUG_PRINTL("keyboard down: " << key)
#endif
    enableControl(afk_core.settings.keyboardControls.get(key));
}

void afk_keyboardUp(const std::string& key)
{
#if DEBUG_CONTROLS
    AFK_DEBUG_PRINTL("keyboard up: " << key)
#endif
    disableControl(afk_core.settings.keyboardControls.get(key));
}

void afk_mouse(unsigned int button)
{
#if DEBUG_CONTROLS
    AFK_DEBUG_PRINTL("mouse down: " << std::dec << button)
#endif
    enableControl(afk_core.settings.mouseControls[button]);
}

void afk_mouseUp(unsigned int button)
{
#if DEBUG_CONTROLS
    AFK_DEBUG_PRINTL("mouse up: " << std::dec << button)
#endif
    disableControl(afk_core.settings.mouseControls[button]);
}

void afk_motion(int x, int y)
{
    if (AFK_TEST_CONTROL_BIT(afk_core.controlsEnabled, AFK_Control::MOUSE_CAPTURE))
    {
        displaceAxis(afk_core.settings.mouseAxisControls[AFK_MouseAxis::X],
            afk_core.settings.mouseAxisSensitivity * static_cast<float>(x));
        displaceAxis(afk_core.settings.mouseAxisControls[AFK_MouseAxis::Y],
            afk_core.settings.mouseAxisSensitivity * static_cast<float>(y));
    }
}

