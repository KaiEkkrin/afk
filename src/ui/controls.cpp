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

#include <cassert>
#include <sstream>

#include "controls.hpp"

/* Defaults.
 * I configure the defaults in a function, rather than as a static global
 * variable, to work around issues with things being initialised in the
 * wrong order.
 */
std::list<struct AFK_DefaultControl>& afk_getDefaultControls(void)
{
    static std::list<struct AFK_DefaultControl> defaultControls = {
        { AFK_Control::THRUST_FORWARD, "thrustForward", AFK_InputType::KEYBOARD, "wW" },
        { AFK_Control::THRUST_BACKWARD, "thrustBackward", AFK_InputType::KEYBOARD, "sS" },
        { AFK_Control::THRUST_RIGHT, "thrustRight", AFK_InputType::KEYBOARD, "dD" },
        { AFK_Control::THRUST_LEFT, "thrustLeft", AFK_InputType::KEYBOARD, "aA" },
        { AFK_Control::THRUST_UP, "thrustUp", AFK_InputType::KEYBOARD, "rR" },
        { AFK_Control::THRUST_DOWN, "thrustDown", AFK_InputType::KEYBOARD, "fF" },
        { AFK_Control::PITCH_UP, "pitchUp", AFK_InputType::NONE, "" },
        { AFK_Control::PITCH_DOWN, "pitchDown", AFK_InputType::NONE, "" },
        { AFK_Control::ROLL_RIGHT, "rollRight", AFK_InputType::NONE, "" },
        { AFK_Control::ROLL_LEFT, "rollLeft", AFK_InputType::NONE, "" },
        { AFK_Control::YAW_RIGHT, "yawRight", AFK_InputType::KEYBOARD, "eE" },
        { AFK_Control::YAW_LEFT, "yawLeft", AFK_InputType::KEYBOARD, "qQ" },
        { AFK_Control::AXIS_PITCH, "pitchAxis", AFK_InputType::MOUSE_AXIS, "Y" },
        { AFK_Control::AXIS_ROLL, "rollAxis", AFK_InputType::MOUSE_AXIS, "X" },
        { AFK_Control::AXIS_YAW, "yawAxis", AFK_InputType::NONE, "" },
        { AFK_Control::PRIMARY_FIRE, "primaryFire", AFK_InputType::MOUSE, "1" },
        { AFK_Control::SECONDARY_FIRE, "secondaryFire", AFK_InputType::MOUSE, "3" },
        { AFK_Control::MOUSE_CAPTURE, "mouseCapture", AFK_InputType::KEYBOARD, "mM" },
        { AFK_Control::MOUSE_CAPTURE, "mouseCapture", AFK_InputType::MOUSE, "2" },
        { AFK_Control::FULLSCREEN, "fullscreen", AFK_InputType::KEYBOARD, "0)" }, /* TODO separate control type for control keys f11 etc? */
    };

    return defaultControls;
}

/* AFK_ConfigOptionControl implementation */

void AFK_ConfigOptionControl::setupDefaultMapping(void)
{
    /* Map the relevant controls from the default control list. */
    for (auto d : afk_getDefaultControls())
    {
        if (d.defaultType == inputType)
        {
            map(d.defaultValue, d.control);
        }
    }
}

std::string AFK_ConfigOptionControl::spellingOf(AFK_Control control) const
{
    /* Iterating through the entire default controls list to find the spelling
     * is not very efficient, but the save() operation isn't time critical,
     * so I don't think I care.
     */
    for (auto d : afk_getDefaultControls())
    {
        if (d.control == control && d.defaultType == inputType)
        {
            return *(name.spellingsBegin()) + *(d.getName().spellingsBegin());
        }
    }

    /* We really shouldn't ever get here */
    assert(false);
    return "UNRECOGNIZED";
}

AFK_ConfigOptionControl::AFK_ConfigOptionControl(const std::string& _name, AFK_InputType _inputType, std::list<AFK_ConfigOptionBase *> *options) :
AFK_ConfigOptionBase(_name, options),
inputType(_inputType)
{
}

bool AFK_ConfigOptionControl::nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg)
{
    /* Rather than matching the whole spelling here, I instead
    * look for the spelling of the base name at the start, followed
    * by the spelling of a valid control name
    */
    std::string arg = getArg();
    size_t baseNameLength;
    if (name.subMatches(arg, 0, baseNameLength))
    {
        for (auto d : afk_getDefaultControls())
        {
            size_t controlNameLength;
            if (d.getName().subMatches(arg, baseNameLength, controlNameLength) &&
                (baseNameLength + controlNameLength) == arg.size())
            {
                nextArg();
                matchedControl = d.control;
                return true;
            }
        }
    }

    return false;
}

bool AFK_ConfigOptionControl::matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg)
{
    map(getArg(), matchedControl);
    nextArg();
    return true;
}

/* AFK_KeyboardControls implementation. */

bool AFK_KeyboardControls::replaceInList(char key, AFK_Control control)
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

void AFK_KeyboardControls::appendToList(char key, AFK_Control control)
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

void AFK_KeyboardControls::saveInternal(std::ostream& os) const
{
    assert(keyList.size() == controlList.size());
    auto keyListIt = keyList.begin();
    auto controlListIt = controlList.begin();
    while (keyListIt != keyList.end())
    {
        os << spellingOf(*(controlListIt++)) << "=" << *(keyListIt++) << std::endl;
    }
}

AFK_KeyboardControls::AFK_KeyboardControls(std::list<AFK_ConfigOptionBase *> *options):
AFK_ConfigOptionControl("keyboard", AFK_InputType::KEYBOARD, options)
{
    setupDefaultMapping();
}

bool AFK_KeyboardControls::map(const std::string& input, AFK_Control control)
{
    // TODO: This only assigns a key to a control.  It ought to also
    // un-assign that key from any other controls...
    if ((static_cast<int>(control) & AFK_CONTROL_AXIS_BIT) != 0) return false;
    for (char key : input)
    {
        if (!replaceInList(key, control)) appendToList(key, control);
    }

    upToDate = false;
    return true;
}

AFK_Control AFK_KeyboardControls::get(const std::string& key)
{
    if (!upToDate) updateMapping();
    size_t pos = keys.find(key);
    if (pos != std::string::npos)
        return controls[pos];
    else
        return AFK_Control::NONE;
}

/* AFK_MouseControls implementation. */

void AFK_MouseControls::saveInternal(std::ostream& os) const
{
    for (auto m : mapping)
    {
        os << spellingOf(m.second) << "=" << boost::lexical_cast<std::string, int>(m.first) << std::endl;
    }
}

AFK_MouseControls::AFK_MouseControls(std::list<AFK_ConfigOptionBase *> *options) :
AFK_ConfigOptionControl("mouse", AFK_InputType::MOUSE, options)
{
    setupDefaultMapping();
}

bool AFK_MouseControls::map(const std::string& input, AFK_Control control)
{
    try
    {
        mapping[boost::lexical_cast<int>(input)] = control;
        return true;
    }
    catch (boost::bad_lexical_cast)
    {
        return false;
    }
}

AFK_Control& AFK_MouseControls::operator[](int button)
{
    return mapping[button];
}

/* AFK_MouseAxisControls implementation. */

void AFK_MouseAxisControls::saveInternal(std::ostream& os) const
{
    for (auto m : mapping)
    {
        os << spellingOf(m.second) << "=";
        switch (m.first)
        {
        case AFK_MouseAxis::X:
            os << "X" << std::endl;
            break;

        case AFK_MouseAxis::Y:
            os << "Y" << std::endl;
            break;

        default:
            /* This really shouldn't happen */
            assert(false);
            os << "UNRECOGNIZED" << std::endl;
            break;
        }
    }
}

AFK_MouseAxisControls::AFK_MouseAxisControls(std::list<AFK_ConfigOptionBase *> *options) :
AFK_ConfigOptionControl("mouseAxis", AFK_InputType::MOUSE_AXIS, options)
{
    setupDefaultMapping();
}

bool AFK_MouseAxisControls::map(const std::string& input, AFK_Control control)
{
    if (toupper(input[0]) == 'X')
    {
        mapping[AFK_MouseAxis::X] = control;
        return true;
    }
    else if (toupper(input[0]) == 'Y')
    {
        mapping[AFK_MouseAxis::Y] = control;
        return true;
    }
    else
    {
        return false;
    }
}

AFK_Control& AFK_MouseAxisControls::operator[](AFK_MouseAxis axis)
{
    return mapping[axis];
}
