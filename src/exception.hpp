/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_EXCEPTION_H_
#define _AFK_EXCEPTION_H_

#include "afk.hpp"

#include <exception>
#include <string>

class AFK_Exception: public std::exception
{
protected:
    std::string message;

public:
    AFK_Exception(const std::string& _message);
    AFK_Exception(const std::string& _message, const GLubyte *_glMessage);
    virtual ~AFK_Exception() throw() {}

    virtual const char* what() const throw();

    const std::string& getMessage() const;
};

#endif /* _AFK_EXCEPTION_H_ */

