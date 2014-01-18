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

#include <sstream>

#include "controls.hpp"

/* Defaults. */
std::list<struct AFK_DefaultControl> afk_defaultControls = {
    {   CTRL_THRUST_FORWARD,    "thrustForward",    INPUT_TYPE_KEYBOARD,    "wW"    },
    {   CTRL_THRUST_BACKWARD,   "thrustBackward",   INPUT_TYPE_KEYBOARD,    "sS"    },
    {   CTRL_THRUST_RIGHT,      "thrustRight",      INPUT_TYPE_KEYBOARD,    "dD"    },
    {   CTRL_THRUST_LEFT,       "thrustLeft",       INPUT_TYPE_KEYBOARD,    "aA"    },
    {   CTRL_THRUST_UP,         "thrustUp",         INPUT_TYPE_KEYBOARD,    "rR"    },
    {   CTRL_THRUST_DOWN,       "thrustDown",       INPUT_TYPE_KEYBOARD,    "fF"    },
    {   CTRL_PITCH_UP,          "pitchUp",          INPUT_TYPE_NONE,        ""      },
    {   CTRL_PITCH_DOWN,        "pitchDown",        INPUT_TYPE_NONE,        ""      },
    {   CTRL_ROLL_RIGHT,        "rollRight",        INPUT_TYPE_NONE,        ""      },
    {   CTRL_ROLL_LEFT,         "rollLeft",         INPUT_TYPE_NONE,        ""      },
    {   CTRL_YAW_RIGHT,         "yawRight",         INPUT_TYPE_KEYBOARD,    "eE"    },
    {   CTRL_YAW_LEFT,          "yawLeft",          INPUT_TYPE_KEYBOARD,    "qQ"    },
//    {   CTRL_AXIS_PITCH,        "pitchAxis",        INPUT_TYPE_MOUSE_AXIS,  "X"     },
 //   {   CTRL_AXIS_ROLL,         "rollAxis",         INPUT_TYPE_MOUSE_AXIS,  "Y"     },
  //  {   CTRL_AXIS_YAW,          "yawAxis",          INPUT_TYPE_NONE,        ""      }, /* TODO sort out treatment of axes */
    {   CTRL_PRIMARY_FIRE,      "primaryFire",      INPUT_TYPE_MOUSE,       "1"     },
    {   CTRL_SECONDARY_FIRE,    "secondaryFire",    INPUT_TYPE_MOUSE,       "3"     },
    {   CTRL_MOUSE_CAPTURE,     "mouseCapture",     INPUT_TYPE_KEYBOARD,    "mM"    },
    {   CTRL_MOUSE_CAPTURE,     "mouseCapture",     INPUT_TYPE_MOUSE,       "2"     },
    {   CTRL_FULLSCREEN,        "fullscreen",       INPUT_TYPE_KEYBOARD,    "0)"    }, /* TODO separate control type for control keys f11 etc? */
};

/* AFK_KeyboardControls implementation.  WIP -- disabled for now */

bool AFK_KeyboardControls::replaceInList(char key, enum AFK_Controls control)
{
    auto keyIt = keyList.begin();
    auto controlIt = controlList.begin();

    while (keyIt != keyList.end())
    {
        if (*keyIt == key)
        {
            *controlIt = control;
            return true;
        }

        ++keyIt;
        ++controlIt;
    }

    assert(controlIt == controlList.end());
    return false;
}

void AFK_KeyboardControls::appendToList(char key, enum AFK_Controls control)
{
    keyList.push_back(key);
    controlList.push_back(control);
}

void AFK_KeyboardControls::updateMapping(void)
{
    std::ostringstream keySS;
    for (auto key : keyList) keySS << key;
    keys = keySS.str();

    controls.clear();
    controls.insert(controls.end(), controlList.begin(), controlList.end());

    upToDate = true;
}

AFK_KeyboardControls::AFK_KeyboardControls(std::list<AFK_ConfigOptionBase *> *options):
AFK_ConfigOptionBase("Keyboard", options)
{
    /* Map the keyboard controls from the default control list. */
    for (auto d : afk_defaultControls)
    {
        if (d.defaultType == INPUT_TYPE_KEYBOARD)
        {
            map(d.defaultValue, d.control);
        }
    }
}

void AFK_KeyboardControls::map(const std::string& keys, enum AFK_Controls control)
{
    for (char key : keys)
    {
        if (!replaceInList(key, control)) appendToList(key, control);
    }

    upToDate = false;
}

bool AFK_KeyboardControls::nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg)
{
    /* Rather than matching the whole spelling here, I instead
     * look for the spelling of "Keyboard" at the start, followed
     * by the spelling of a valid control name
     */
    std::string arg = getArg();
    size_t keyboardLength;
    if (name.subMatches(arg, 0, keyboardLength))
    {
        for (auto d : afk_defaultControls)
        {
            size_t keyLength;
            if (d.getName().subMatches(arg, keyboardLength, keyLength) &&
                (keyboardLength + keyLength) == arg.size())
            {
                nextArg();
                matchedControl = d.control;
                return true;
            }
        }
    }

    return false;
}

bool AFK_KeyboardControls::matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg)
{
    map(getArg(), matchedControl);
    nextArg();
    return true;
}

enum AFK_Controls AFK_KeyboardControls::operator[](const std::string& key)
{
    if (!upToDate) updateMapping();
    size_t pos = keys.find(key);
    if (pos != std::string::npos)
        return controls[pos];
    else
        return CTRL_NONE;
}

void AFK_KeyboardControls::save(std::ostream& os) const
{
    /* TODO. */
}

