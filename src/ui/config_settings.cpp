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

#include <fstream>
#include <iostream>

#include <boost/tokenizer.hpp>

#include "config_settings.hpp"

/* AFK_ConfigSettings implementation */

void AFK_ConfigSettings::loadConfigFromFile(void)
{
    std::ifstream cf;
    try
    {
        cf.open(configFile->get(), std::ios::in);
        std::string line;
        int lineNum = 0;

        /* We separate the lines by "=" and ";".
        * TODO: Handle ";" differently -- use it as a comment character
        */
        boost::char_separator<char> configSep("=;");

        while (std::getline(cf, line))
        {
            boost::tokenizer<boost::char_separator<char> > lineTok(line, configSep);
            auto lineIt = lineTok.begin();
            auto lineEnd = lineTok.end();

            /* I'm going to assume there is just one option per line.
             */
            bool lineParsed = false;
            for (auto option : options)
            {
                if (option->parseFileLine(lineIt, lineEnd))
                {
                    lineParsed = true;
                    break;
                }
            }

            if (!lineParsed)
            {
                /* If the line isn't empty, complain */
                if (boost::algorithm::trim_copy(line).size() > 0)
                {
                    std::cout << "AFK load settings: Parse error at line " << lineNum << ": " << line << std::endl;
                }
            }

            ++lineNum;
        }

        cf.close();
        std::cout << "AFK: Loaded configuration from " << configFile->get() << std::endl;
    }
    catch (std::ios_base::failure& failure)
    {
        std::cout << "AFK: Failed to load configuration from " << configFile->get() << ": " << failure.what() << std::endl;
    }
}

float AFK_ConfigSettings::getAxisInversion(AFK_Control axis) const
{
    assert((static_cast<int>(axis) & AFK_CONTROL_AXIS_BIT) != 0);
    switch (axis)
    {
    case AFK_Control::AXIS_PITCH:
        return pitchAxisInverted ? -1.0f : 1.0f;

    case AFK_Control::AXIS_ROLL:
        return rollAxisInverted ? -1.0f : 1.0f;

    case AFK_Control::AXIS_YAW:
        return yawAxisInverted ? -1.0f : 1.0f;

    default:
        return 0.0f;
    }
}

AFK_ConfigSettings::AFK_ConfigSettings()
{
    /* TODO: The config file should default to a dot-path on GNU
     * (".afk/afk.config") or a location in the user's documents
     * directory on Windows ("afk/afk.config") but for now I'm just
     * going to use the CWD to test.
     */
    configFile = new AFK_ConfigOption<std::string>("configFile", &options, "afk.config", true);

    /* Load configuration from file right away.  Any command line arguments
     * will be parsed later and override this.
     */
    loadConfigFromFile();
}

AFK_ConfigSettings::~AFK_ConfigSettings()
{
    if (saveOnQuit)
    {
        /* TODO: Move the old config file out of the way, etc. */
        std::ofstream cf;
        try
        {
            cf.open(configFile->get(), std::ios::out | std::ios::trunc);
            for (auto option : options)
            {
                option->save(cf);
            }

            cf.close();
            std::cout << "AFK: Saved configuration to " << configFile->get() << std::endl;
        }
        catch (std::ios_base::failure& failure)
        {
            std::cout << "AFK: Failed to save configuration to " << configFile->get() << ": " << failure.what() << std::endl;
        }
    }
}

bool AFK_ConfigSettings::parseCmdLine(int *argcp, char **argv)
{
    char **argvHere = argv + 1; /* skip process name */
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
