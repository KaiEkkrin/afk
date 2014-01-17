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

/* AFK_KeyboardControls implementation.  WIP -- disabled for now */
#if 0

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

}

void AFK_KeyboardControls::map(const std::string& keys, enum AFK_Controls control)
{
    for (char key : keys)
    {
        if (!replaceInList(key, control)) appendToList(key, control);
    }

    upToDate = false;
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
#endif
