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

#include <iomanip>

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
    spelling1[0] = static_cast<char>(toupper(spelling1[0]));
    spellings.push_back(spelling1);

    /* The command-line switch is made by adding `-' to the front,
     * and replacing uppercase characters with `-<lowercase>'.
     */
    std::ostringstream cmdlineSS;
    cmdlineSS << "--";
    for (char c : name)
    {
        if (isupper(c)) cmdlineSS << "-" << static_cast<char>(tolower(c));
        else cmdlineSS << c;
    }

    /* If you add any more spellings to the list, getCmdLineSpelling()
     * below will need changing!
     */

    spellings.push_back(cmdlineSS.str());
}

const std::string& AFK_ConfigOptionName::getFileSpelling(void) const
{
    return spellings.front();
}

const std::string& AFK_ConfigOptionName::getCmdLineSpelling(void) const
{
    return spellings.back();
}

bool AFK_ConfigOptionName::matches(const std::string& arg)
{
    for (auto spelling : spellings)
    {
        if (arg == spelling) return true;
    }

    return false;
}

bool AFK_ConfigOptionName::subMatches(const std::string& arg, size_t start, size_t& o_matchedLength)
{
    for (auto spelling : spellings)
    {
        if (spelling.size() <= (arg.size() - start) &&
            arg.compare(start, spelling.size(), spelling) == 0)
        {
            o_matchedLength = spelling.size();
            return true;
        }
    }

    return false;
}

std::ostream& operator<<(std::ostream& os, const AFK_ConfigOptionName& optionName)
{
    return os << optionName.spellings.front();
}

/* AFK_ConfigOptionBase implementation */

void AFK_ConfigOptionBase::printPaddedHelpLine(std::ostream& os, const std::string& subject, const std::string& prefix, const std::string& annotation) const
{
    os << std::setw(40) << subject << ": " << prefix << help << " " << annotation << std::endl;
}

AFK_ConfigOptionBase::AFK_ConfigOptionBase(const std::string& _name, const std::string& _help, std::list<AFK_ConfigOptionBase *> *options, bool _noSave) :
    name(_name),
    help(_help),
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

void AFK_ConfigOptionBase::save(std::ostream& os) const
{
    if (!noSave) saveInternal(os);
}
