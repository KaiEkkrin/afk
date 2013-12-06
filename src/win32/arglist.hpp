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

#ifndef _AFK_WIN32_ARGLIST_H_
#define _AFK_WIN32_ARGLIST_H_

#ifdef _WIN32

/* Utility for getting Unix-style command line arguments from Windows. */

#include <Windows.h>
#include <shellapi.h>

class AFK_ArgList
{
protected:
    LPWSTR *argvw;

    int argc;
    char **argv;

public:
    AFK_ArgList();
    virtual ~AFK_ArgList();


    int getArgc(void) const { return argc; }
    char **getArgv(void) const { return argv; }
};

#endif /* _WIN32 */
#endif /* _AFK_WIN32_ARGLIST_H_ */
