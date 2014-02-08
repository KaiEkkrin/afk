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
    /* True if we're logging to the console, else false. */
    bool console;

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

    void setConsole(bool _console);
    bool setLogFile(const std::string& logFile);

    /* Returns the amount to pbump() for, or 0 on failure. */
    int doWrite(const char *start, const char *end);
};

/* The AFK log looks like a std::ostream, but logs to the console or
 * tee's to a file too, bypassing the problems I have with the C++ output
 * streams on Windows.
 */
class AFK_LogStream : public std::streambuf, public std::ostream
{
protected:
    /* We buffer a little bit of data here. */
    std::array<char, AFK_LOGSTREAM_BUF_SIZE> buf;

    /* streambuf implementation: */
    int overflow(int ch) override;
    int sync(void) override;

public:
    AFK_LogStream();
    virtual ~AFK_LogStream();
};

/* Here's the global log backing and its mutex: */
extern AFK_LogBacking afk_logBacking;
extern std::mutex afk_logBackingMut;

/* Here's a thing to use in place of std::cout / std::cerr.
 * TODO: Right now, this is horribly thread unsafe in a way I can't fix easily
 * (because I'd need to change all the operator<< functions for all the types;
 * eww, C++ polymorphism fail).  A workaround would be to create thread local
 * versions, but that would be messy because thread local objects can't have
 * constructors or destructors!  For now, use this only from the main thread,
 * and use the log backing directly (see the `debug' module) from other threads.
 */
extern AFK_LogStream afk_out;

/* TODO: If I make a no-console version, disable this / return right away or something;
 * I can snoop afk_out to find out whether we have a console output
 */
void afk_waitForKeyPress(void);

#endif /* _AFK_UI_LOGSTREAM_H_ */
