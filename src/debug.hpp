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

#ifndef _AFK_DEBUG_H_
#define _AFK_DEBUG_H_

#include <iostream>
#include <sstream>

void afk_debugPrint(const std::string& s);

#define AFK_DEBUG_PRINT(expr) \
    { \
        std::ostringstream ss; \
        ss << expr; \
        afk_debugPrint(ss.str()); \
    }

#define AFK_DEBUG_PRINTL(expr) \
    { \
        std::ostringstream ss; \
        ss << expr << std::endl; \
        afk_debugPrint(ss.str()); \
    }

#endif /* _AFK_DEBUG_H_ */

