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

#include "afk.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>

#include "exception.hpp"

#ifdef __GNUC__
/* TODO: Right now this backtrace is completely unreadable and useless :(
 * I'd like one, but...
 * Maybe there are better ones to be found (on Windows, and on Linux?)
 */
#include <execinfo.h>
#define BACKTRACE_BUF_SIZE 4096

void AFK_Exception::getBacktrace(void)
{
    backtraceBuf = (void **)malloc(BACKTRACE_BUF_SIZE);
    backtraceSize = backtrace(backtraceBuf, BACKTRACE_BUF_SIZE);
}
#else
void AFK_Exception::getBacktrace(void) {}
#endif /* __GNUC__ */

AFK_Exception::AFK_Exception(const std::string& _message):
    backtraceBuf(nullptr)
{
    message = std::string(_message);
    getBacktrace();
}

AFK_Exception::AFK_Exception(const std::string& _message, const GLubyte *_glMessage):
    backtraceBuf(nullptr)
{
    /* It looks like C++ chokes trying to print that ubyte :-( */
    char *msgChars = nullptr;
    size_t msgCharsLength = _message.length() + 4;
    size_t glMessageLength;
    for (glMessageLength = 0; _glMessage[glMessageLength] != 0; ++glMessageLength);
    msgCharsLength += glMessageLength;

    msgChars = (char *)malloc(msgCharsLength * sizeof(char));
    if (msgChars)
    {
#ifdef __GNUC__
        snprintf(msgChars, msgCharsLength, "%s: %s", _message.c_str(), _glMessage);
#endif
#ifdef _WIN32
        sprintf_s(msgChars, msgCharsLength, "%s: %s", _message.c_str(), _glMessage);
#endif
        message = std::string(msgChars);
        free(msgChars);
    }
    else
    {
        message = std::string(_message + ": (Failed to get GL message)");
    }

    getBacktrace();
}

AFK_Exception::~AFK_Exception() throw()
{
}

const char *AFK_Exception::what() const throw()
{
    /* If this function is called, chances are I'll want to read
     * the backtrace.
     */
    std::cerr << "what() called on exception with backtrace: " << std::endl;
    printBacktrace();
    std::cerr << std::endl;

    return message.c_str();
}

const std::string& AFK_Exception::getMessage() const
{
    return message;
}

void AFK_Exception::printBacktrace(void) const
{
#ifdef __GNUC__
    if (backtraceBuf)
        backtrace_symbols_fd(backtraceBuf, backtraceSize, fileno(stderr));
#endif
}
