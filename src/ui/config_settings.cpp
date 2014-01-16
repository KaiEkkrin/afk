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

#include <iostream>

#include <boost/tokenizer.hpp>

#include "config_settings.hpp"

/* AFK_ConfigSettings implementation */

AFK_ConfigSettings::AFK_ConfigSettings()
{
    // TODO.
}

bool AFK_ConfigSettings::parseCmdLine(int *argcp, char **argv)
{
    char **argvHere = argv;
    char **argvEnd = argv + *argcp;
    while (argvHere < argvEnd)
    {
        bool matched = false;
        for (auto option : options)
        {
            matched = option->parseCmdArgs(argvHere, argvEnd);
            if (matched) break;
        }

        if (!matched)
        {
            std::cerr << "AFK_ConfigSettings::parseCmdLine : Unmatched option: " << *argvHere << std::endl;
            return false;
        }
    }

    *argcp = 0; // ??
    return true;
}

bool AFK_ConfigSettings::parseFile(char **buf, size_t bufSize)
{
    /* I want to split config files by = and ; */
    boost::char_separator<char> configSep("=;");

    /* That config file came in line by line */
    for (size_t lineI = 0; lineI < bufSize; ++lineI)
    {
        std::string line(buf[lineI]);
        boost::tokenizer<boost::char_separator<char> > lineTok(line, configSep);
        auto lineIt = lineTok.begin();
        auto lineEnd = lineTok.end();

        /* This time, I'm going to assume only one option per line,
         * and just ignore it if it doesn't parse (assume a comment,
         * etc)
         */
        for (auto option : options)
        {
            if (option->parseFileLine(lineIt, lineEnd)) break;
        }
    }

    return true;
}
