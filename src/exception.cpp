/* AFK (c) Alex Holloway 2013 */

#include "afk.hpp"

#include <stdio.h>
#include <stdlib.h>

#include "exception.hpp"

AFK_Exception::AFK_Exception(const std::string& _message)
{
    message = std::string(_message);
}

AFK_Exception::AFK_Exception(const std::string& _message, const GLubyte *_glMessage)
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
}

const char *AFK_Exception::what() const throw()
{
    return message.c_str();
}

