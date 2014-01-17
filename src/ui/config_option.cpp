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

#include "config_option.hpp"

/* AFK_ConfigOptionName implementation */

AFK_ConfigOptionName::AFK_ConfigOptionName(const std::string& _name):
    name(_name)
{
    assert(islower(_name[0]));

    /* The first spelling is made by capitalising the first letter of the
     * field name
     */
    std::string spelling1 = _name;
    spelling1[0] = toupper(spelling1[0]);
    spellings.push_back(spelling1);

    /* The command-line switch is made by adding `-' to the front,
     * and replacing uppercase characters with `-<lowercase>'.
     */
    std::ostringstream cmdlineSS;
    cmdlineSS << "-";
    for (char c : name)
    {
        if (isupper(c)) cmdlineSS << "-" << tolower(c);
        else cmdlineSS << c;
    }

    spellings.push_back(cmdlineSS.str());
}

bool AFK_ConfigOptionName::matches(const std::string& arg)
{
    for (auto spelling : spellings)
    {
        if (arg == spelling) return true;
    }

    return false;
}

std::ostream& operator<<(std::ostream& os, const AFK_ConfigOptionName& optionName)
{
    return os << optionName.spellings.front();
}

/* AFK_ConfigOptionBase implementation */

AFK_ConfigOptionBase::AFK_ConfigOptionBase(const std::string& _name, std::list<AFK_ConfigOptionBase *> *options, bool _noSave) :
    name(_name),
    noSave(_noSave)
{
    if (options) options->push_back(this);
}

bool AFK_ConfigOptionBase::nameMatches(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg)
{
    if (name.matches(getArg()))
    {
        nextArg();
        return true;
    }

    return false;
}
