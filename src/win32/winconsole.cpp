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
#include <iostream>

#include "winconsole.hpp"

AFK_WinConsole::AFK_WinConsole():
outf(nullptr), errf(nullptr)
{
    if (!AllocConsole())
    {
        /* That really ought to succeed. */
        assert(false);
    }
    else
    {
        resync();
    }
}

AFK_WinConsole::~AFK_WinConsole()
{
    /* TODO: I could close stuff here, but I fear the destructors for
     * other stuff will run later and try to print, and things will
     * splat ...
     */
}

void AFK_WinConsole::resync()
{
    /* Reopen the standard streams to these magic Windows names */
    freopen_s(&outf, "CONIN$", "r", stdin);
    freopen_s(&outf, "CONOUT$", "w", stdout);
    freopen_s(&errf, "CONOUT$", "w", stderr);

    /* Sync up C++ I/O objects...
     * Except this doesn't appear to work, or isn't sufficient, in the
     * presence of various file I/O (especially the configuration file),
     * which is part of the motivation behind the logstream module.
     */
    //std::ios::sync_with_stdio();
}

#endif /* _WIN32 */
