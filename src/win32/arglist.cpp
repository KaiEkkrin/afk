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

#ifdef _WIN32

#include <cassert>
#include <sstream>

#include "arglist.hpp"
#include "../exception.hpp"

AFK_ArgList::AFK_ArgList():
argvw(nullptr), argc(0), argv(nullptr)
{
    /* Using the shellapi, I can extract a Unix style argv, albeit only in wide char format: */
    LPWSTR fullCmdLineW = GetCommandLineW();
    assert(fullCmdLineW);

    argvw = CommandLineToArgvW(fullCmdLineW, &argc);
    if (!argvw)
    {
        std::ostringstream ss;
        ss << "CommandLineToArgvW failed: " << GetLastError();
        throw AFK_Exception(ss.str());
    }

    /* Convert those wide strings into something sane.
    * TODO: Yes, I know, this means AFK isn't supporting unicode paths.
    * I need to come up with something sensible, like converting to UTF-8
    * here.
    */
    argv = new char *[argc];
    for (int i = 0; i < argc; ++i)
    {
        /* Work out how much space I need for this argument */
        int requiredSize = WideCharToMultiByte(
            CP_ACP,
            0,
            argvw[i],
            -1, /* CommandLineToArgvW must produce null terminated strings */
            nullptr,
            0,
            0,
            0
            );

        if (requiredSize > 0)
        {
            argv[i] = new char[requiredSize];
            requiredSize = WideCharToMultiByte(
                CP_ACP,
                0,
                argvw[i],
                -1,
                argv[i],
                requiredSize,
                0,
                0
                );
        }

        assert(requiredSize > 0);
        if (requiredSize == 0)
        {
            argv[i] = new char[1];
            argv[i][0] = '\0';
        }
    }
}

AFK_ArgList::~AFK_ArgList()
{
    if (argv)
    {
        for (int i = 0; i < argc; ++i) delete[] argv[i];
        delete[] argv;
    }

    if (argvw) LocalFree(argvw);
}

#endif /* _WIN32 */
