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

#include <iostream>

#include <boost/tokenizer.hpp>

#include "config_settings.hpp"
#include "../exception.hpp"
#include "../file/logstream.hpp"
#include "../file/readfile.hpp"

/* AFK_ConfigSettings implementation */

void AFK_ConfigSettings::loadConfigFromFile(void)
{
    char *data = nullptr;
    size_t dataSize = 0;

    std::ostringstream errorSS;
    if (afk_readFileContents(configFile->get(), &data, &dataSize, errorSS))
    {
        std::string dataStr(data, dataSize);

        boost::char_separator<char> lineSep("\r\n");
        boost::tokenizer<boost::char_separator<char> > fileTok(dataStr, lineSep);

        /* We separate the lines by "=" and ";".
        * TODO: Handle ";" differently -- use it as a comment character
        */
        boost::char_separator<char> configSep("=;");
        int lineNum = 0;

        for (auto line : fileTok)
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
                    errorSS << "AFK load settings: Parse error at line " << lineNum << ": " << line;
                    throw AFK_Exception(errorSS.str());
                }
            }

            ++lineNum;
        }

        free(data);
    }

    /* TODO: report no-such-file errors etc? */
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
        std::ostringstream ss;
        saveTo(ss);

        std::string sstr = ss.str();
        if (afk_writeFileContents(configFile->get(), sstr.c_str(), sstr.size(), afk_out))
        {
            afk_out << "AFK: Saved configuration to " << configFile->get() << std::endl;
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
            afk_out << "AFK_ConfigSettings::parseCmdLine : Unmatched option: " << *argvHere << std::endl;
            return false;
        }
    }

    *argcp = 0; // ??
    return true;
}

void AFK_ConfigSettings::saveTo(std::ostream& os) const
{
    for (auto option : options)
    {
        option->save(os);
    }
}
