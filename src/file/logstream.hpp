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

#ifndef _AFK_UI_LOGSTREAM_H_
#define _AFK_UI_LOGSTREAM_H_

#include <array>
#include <cstdio>
#include <ostream>
#include <streambuf>

#define AFK_LOGSTREAM_BUF_SIZE 256

/* The AFK log looks like a std::ostream, but can log to a file, too;
 * configure that with setLogFile().
 */
class AFK_LogStream : public std::streambuf, public std::ostream
{
protected:
    /* We buffer a little bit of data here. */
    std::array<char, AFK_LOGSTREAM_BUF_SIZE> buf;

    /* If we're tee'ing to a file, here is its handle.
     * Yes, we're using C I/O here.  The reason for this is the
     * Windows console plays havoc with C++ I/O, especially if
     * opening files as well.  The C I/O implementation just
     * seems more reliable.
     */
    FILE *f;

    /* streambuf implementation: */
    int overflow(int ch) override;
    int sync(void) override;

    /* my own innards */
    bool openLogFile(const std::string& logFile);
    void closeLogFile(void);
    bool doWrite(const char *start, const char *end);

public:
    AFK_LogStream();
    virtual ~AFK_LogStream();

    bool setLogFile(const std::string& logFile);
};

/* Here's the thing to use in place of std::cout / std::cerr, then. */
extern AFK_LogStream afk_out;

#endif /* _AFK_UI_LOGSTREAM_H_ */
