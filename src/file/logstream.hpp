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
#include <exception>
#include <mutex>
#include <ostream>
#include <streambuf>

/* TODO: The streambuf seems to be surprisingly dumb, and splat when
 * more than this gets thrown at it at once (!)
 */
#define AFK_LOGSTREAM_BUF_SIZE 4096

/* This is the backing stuff for the AFK log stream.  It should be
 * unique.  It's not thread safe, that needs to be managed by the
 * things that refer to it.
 */
class AFK_LogBacking
{
protected:
    /* If we're tee'ing to a file, here is its handle.
    * Yes, we're using C I/O here.  The reason for this is the
    * Windows console plays havoc with C++ I/O, especially if
    * opening files as well.  The C I/O implementation just
    * seems more reliable.
    */
    FILE *f;

    /* my own innards */
    bool openLogFile(const std::string& logFile);
    void closeLogFile(void);

public:
    AFK_LogBacking();
    virtual ~AFK_LogBacking();

    bool setLogFile(const std::string& logFile);

    /* Returns the amount to pbump() for, or 0 on failure. */
    int doWrite(const char *start, const char *end);
};

/* The AFK log looks like a std::ostream, but logs to the console or
 * tee's to a file too, bypasses the problems I have with the C++ output
 * streams on Windows, and is internally thread safe (which isn't the
 * same as externally thread safe due to all the operator<< overloads that
 * I can't replace; I wish C++ had real polymorphism! Anyway, you'll still
 * need a lock for sane concurrent access, but it shouldn't puke if you
 * don't, it just may interleave your strings.)
 */
class AFK_LogStream : public std::streambuf, public std::ostream
{
protected:
    AFK_LogBacking backing;
    std::mutex backingMut;

    /* We buffer a little bit of data here. */
    std::array<char, AFK_LOGSTREAM_BUF_SIZE> buf;

    /* streambuf implementation: */
    int overflow(int ch) override;
    int sync(void) override;

public:
    AFK_LogStream();
    virtual ~AFK_LogStream();

    /* Calling this sets the log file for any backing. */
    bool setLogFile(const std::string& logFile);
};

/* Here's the thing to use in place of std::cout / std::cerr, then. */
extern AFK_LogStream afk_out;

/* TODO: If I make a no-console version, disable this / return right away or something;
 * I can snoop afk_out to find out whether we have a console output
 */
void afk_waitForKeyPress(void);

#endif /* _AFK_UI_LOGSTREAM_H_ */
