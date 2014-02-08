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

#include <thread>

#include "debug.hpp"
#include "file/logstream.hpp"

void afk_debugPrint(const std::string& s)
{
    const char *start = s.c_str();
    const char *end = start + s.size();

    std::unique_lock<std::mutex> lock(afk_logBackingMut);
    afk_logBacking.doWrite(start, end);
}
