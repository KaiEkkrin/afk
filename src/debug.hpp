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

#include <cstring>
#include <iomanip>
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

template<typename T, size_t innerBytes = 16>
class AFK_InnerDebug
{
public:
    const T *objPtr;
    unsigned char b[innerBytes];
    const size_t size;

    AFK_InnerDebug(const T *_objPtr):
        objPtr(_objPtr),
        size(std::min(innerBytes, sizeof(T)))
    {
        memcpy(&b[0], objPtr, size);
    }
};

template<typename T, size_t innerBytes = 16>
std::ostream& operator<<(
    std::ostream& os,
    const AFK_InnerDebug<T, innerBytes>& inner)
{
    os << "inside: " << std::hex << inner.objPtr << " -> ";
    for (size_t iBI = 0; iBI < inner.size; ++iBI)
        os << std::hex << std::setfill('0') << std::setw(2) << (unsigned int)inner.b[iBI] << " ";
    return os;
}

#endif /* _AFK_DEBUG_H_ */

