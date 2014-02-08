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

#include <cassert>

#include "logstream.hpp"
#include "readfile.hpp"


/* AFK_LogBacking implementation */

bool AFK_LogBacking::openLogFile(const std::string& logFile)
{
#ifdef _WIN32
    errno_t openErr = fopen_s(&f, logFile.c_str(), "w");
    if (openErr != 0)
    {
        /* TODO: A bit unfriendly, this */
        return false;
    }
#else
    f = fopen(logFile.c_str(), "w");
    if (!f)
    {
        /* TODO: A bit unfriendly, this */
        return false;
    }
#endif

    return true;
}

void AFK_LogBacking::closeLogFile(void)
{
    if (f)
    {
        fclose(f);
        f = nullptr;
    }
}

AFK_LogBacking::AFK_LogBacking() : f(nullptr) {}
AFK_LogBacking::~AFK_LogBacking()
{
    closeLogFile();
}

bool AFK_LogBacking::setLogFile(const std::string& logFile)
{
    closeLogFile();
    return openLogFile(logFile);
}

int AFK_LogBacking::doWrite(const char *start, const char *end)
{
    std::ptrdiff_t size = end - start;
    assert(size >= 0);

    bool success = true;
    if (size > 0)
    {
        success &= (fwrite(start, 1, size, stdout) == static_cast<size_t>(size));
        if (f)
        {
            success &= (fwrite(start, 1, size, f) == static_cast<size_t>(size));
        }
    }

    return (success ? static_cast<int>(-size) : 0);
}


/* AFK_LogStream implementation */

int AFK_LogStream::overflow(int ch)
{
    bool success = false;

    if (ch != std::streambuf::traits_type::eof())
    {
        std::unique_lock<std::mutex> lock(backingMut);

        int bump = backing.doWrite(pbase(), pptr());
        if (bump != 0)
        {
            char last = static_cast<char>(ch);
            int lastBump = backing.doWrite(&last, (&last) + 1);
            if (lastBump != 0) success = true;
            if (success) pbump(bump + lastBump);
        }
    }

    return success ? ch : std::streambuf::traits_type::eof();
}

int AFK_LogStream::sync(void)
{
    std::unique_lock<std::mutex> lock(backingMut);
    int bump = backing.doWrite(pbase(), pptr());
    if (bump != 0)
    {
        pbump(bump);
        return 0;
    }
    else return -1;
}

AFK_LogStream::AFK_LogStream():
std::streambuf(),
std::ostream(this),
backing()
{
    setp(buf.data(), buf.data() + buf.size());
}

AFK_LogStream::~AFK_LogStream()
{
    sync();
}

bool AFK_LogStream::setLogFile(const std::string& logFile)
{
    std::unique_lock<std::mutex> lock(backingMut);
    return backing.setLogFile(logFile);
}

AFK_LogStream afk_out;

void afk_waitForKeyPress(void)
{
    getchar();
}
