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

#ifndef _AFK_EXCEPTION_H_
#define _AFK_EXCEPTION_H_

#include "afk.hpp"

#include <exception>
#include <string>

class AFK_Exception: public std::exception
{
protected:
    std::string message;
    void **backtraceBuf;
    int backtraceSize;

    void getBacktrace(void);

public:
    AFK_Exception(const std::string& _message);
    AFK_Exception(const std::string& _message, const GLubyte *_glMessage);
    virtual ~AFK_Exception() throw();

    virtual const char* what() const throw();

    const std::string& getMessage() const;
    void printBacktrace(void) const;
};

#endif /* _AFK_EXCEPTION_H_ */

