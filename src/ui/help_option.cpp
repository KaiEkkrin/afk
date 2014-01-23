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
#include <iostream>

#include "help_option.hpp"
#include "../file/logstream.hpp"

/* AFK_ConfigOptionHelp implementation */

void AFK_ConfigOptionHelp::saveInternal(std::ostream& os) const
{
    /* This shouldn't be a thing. */
    assert(false);
}

AFK_ConfigOptionHelp::AFK_ConfigOptionHelp(std::list<AFK_ConfigOptionBase *> *_options) :
AFK_ConfigOptionBase("help", "Print this message", _options, true),
options(_options)
{
}

bool AFK_ConfigOptionHelp::matched(std::function<std::string(void)>& getArg, std::function<void(void)>& nextArg)
{
    afk_out << std::endl;
    afk_out << "AFK options:" << std::endl;
    afk_out << "============" << std::endl << std::endl;

    /* Print everything's help message. */
    for (auto option : *options)
    {
        option->printHelp(afk_out);
    }

    afk_out << std::endl;

    /* I want to quit at this point: */
    return false;
}

void AFK_ConfigOptionHelp::printHelp(std::ostream& os) const
{
    printPaddedHelpLine(os, name.getCmdLineSpelling(), "", "");
}
