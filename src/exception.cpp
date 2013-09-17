/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <iostream>
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>

#include "exception.hpp"

#define BACKTRACE_BUF_SIZE 4096

void AFK_Exception::getBacktrace(void)
{
    backtraceBuf = (void **)malloc(BACKTRACE_BUF_SIZE);
    backtraceSize = backtrace(backtraceBuf, BACKTRACE_BUF_SIZE);
}

AFK_Exception::AFK_Exception(const std::string& _message):
    backtraceBuf(NULL)
{
    message = std::string(_message);
    getBacktrace();
}

AFK_Exception::AFK_Exception(const std::string& _message, const GLubyte *_glMessage):
    backtraceBuf(NULL)
{
    /* It looks like C++ chokes trying to print that ubyte :-( */
    char *msgChars = NULL;
    size_t msgCharsLength = _message.length() + 4;
    size_t glMessageLength;
    for (glMessageLength = 0; _glMessage[glMessageLength] != 0; ++glMessageLength);
    msgCharsLength += glMessageLength;

    msgChars = (char *)malloc(msgCharsLength * sizeof(char));
    if (msgChars)
    {
        snprintf(msgChars, msgCharsLength, "%s: %s", _message.c_str(), _glMessage);
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
    if (backtraceBuf) free(backtraceBuf);
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
    if (backtraceBuf)
        backtrace_symbols_fd(backtraceBuf, backtraceSize, fileno(stderr));
}

