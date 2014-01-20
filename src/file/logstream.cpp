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

/* AFK_LogStream implementation */

int AFK_LogStream::overflow(int ch)
{
    bool success = false;

    if (ch != std::streambuf::traits_type::eof())
    {
        success = doWrite(pbase(), pptr());
        if (success)
        {
            char last = static_cast<char>(ch);
            success &= doWrite(&last, (&last) + 1);
        }
    }

    return success ? ch : std::streambuf::traits_type::eof();
}

int AFK_LogStream::sync(void)
{
    return doWrite(pbase(), pptr()) ? 0 : -1;
}

bool AFK_LogStream::openLogFile(const std::string& logFile)
{
#ifdef _WIN32
    errno_t openErr = fopen_s(&f, logFile.c_str(), "w");
    if (openErr != 0)
    {
        /* Okay so this looks funny.  But the message has a reasonable
         * chance of ending up somewhere sensible
         */
        *this << "AFK_LogStream: Failed to open " << logFile << ": " << afk_strerror(openErr);
        return false;
    }
#else
    f = fopen(filename.c_str(), "wb");
    if (!f)
    {
        *this << "AFK_LogStream: Failed to open " << filename << ": " << afk_strerror(errno);
        return false;
    }
#endif

    return true;
}

void AFK_LogStream::closeLogFile(void)
{
    sync(); /* and hope */

    if (f)
    {
        fclose(f);
        f = nullptr;
    }
}

bool AFK_LogStream::doWrite(const char *start, const char *end)
{
    ptrdiff_t size = end - start;
    assert(size >= 0);

    bool success = true;
    if (size > 0)
    {
        success &= (fwrite(start, 1, size, stdout) == size);
        if (f)
        {
            success &= (fwrite(start, 1, size, f) == size);
        }
    }

    pbump(static_cast<int>(-size));
    return success;
}

AFK_LogStream::AFK_LogStream():
std::streambuf(),
std::ostream(this),
f(nullptr)
{
    setp(buf.data(), buf.data() + buf.size());
}

AFK_LogStream::~AFK_LogStream()
{
    closeLogFile();
}

bool AFK_LogStream::setLogFile(const std::string& logFile)
{
    closeLogFile();
    return openLogFile(logFile);
}

AFK_LogStream afk_out;
