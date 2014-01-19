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

#ifndef _AFK_WIN32_WINCONSOLE_H_
#define _AFK_WIN32_WINCONSOLE_H_

/* A quickie module for popping up a console for stdout and stderr. */

#ifdef _WIN32

#include <cstdio>
#include <Windows.h>

class AFK_WinConsole
{
protected:
    const int maxLines = 900;
    FILE *outf;
    FILE *errf;

public:
    AFK_WinConsole();
    virtual ~AFK_WinConsole();

    void resync();
};

#endif /* _WIN32 */
#endif /* _AFK_WIN32_WINCONSOLE_H_ */
