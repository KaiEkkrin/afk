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

#include "readfile.hpp"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <stack>


#ifdef __GNUC__
#include <unistd.h>

static std::string afk_strerror(int e)
{
    return std::string(strerror(e));
}

char *afk_getCWD(void)
{
    return get_current_dir_name();
}

bool afk_setCWD(const char *dir)
{
    return (chdir(dir) >= 0);
}

void afk_printCWDError(std::ostream& io_errStream)
{
    io_errStream << "Failed to set working directory: " << strerror(errno) << std::endl;
}

#endif /* __GNUC__ */

#ifdef _WIN32
#include <Windows.h>

static std::string afk_strerror(errno_t e)
{
    static __declspec(thread) char errbuf[256];
    errno_t s = strerror_s<256>(errbuf, e);
    if (s == 0)
    {
        return std::string(errbuf);
    }
    else
    {
        std::ostringstream ss;
        ss << "(strerror_s failed with code " << s << ")";
        return ss.str();
    }
}

char *afk_getCWD(void)
{
    unsigned int cwdLength = GetCurrentDirectoryA(0, nullptr);
    assert(cwdLength > 0);
    if (cwdLength == 0)
    {
        std::cerr << "Failed to get current directory" << std::endl;
        exit(1);
    }

    char *cwd = static_cast<char*>(malloc(cwdLength + 1));
    cwdLength = GetCurrentDirectoryA(cwdLength + 1, cwd);
    assert(cwdLength > 0);
    if (cwdLength == 0)
    {
        std::cerr << "Failed to get current directory" << std::endl;
        exit(1);
    }

    return cwd;
}

bool afk_setCWD(const char *dir)
{
    return (SetCurrentDirectoryA(dir) != FALSE);
}

void afk_printCWDError(std::ostream& io_errStream)
{
    /* TODO Pretty printed error string */
    io_errStream << "Failed to set working directory: " << GetLastError() << std::endl;
}
#endif /* _WIN32 */

/* The directory stack. */
std::stack<std::string> dirStack;

bool afk_pushDir(const std::string& path, std::ostream& io_errStream)
{
    /* We switch into `path', first pushing the current directory
     * onto the stack.
     */
    char *currentDir = afk_getCWD();
    std::string currentDirStr(currentDir);
    if (!afk_setCWD(path.c_str()))
    {
        afk_printCWDError(io_errStream);
        return false;
    }

    dirStack.push(currentDirStr);
    free(currentDir);
    return true;
}

bool afk_popDir(std::ostream& io_errStream)
{
    if (dirStack.empty())
    {
        io_errStream << "Directory stack empty";
        return false;
    }

    std::string newDir = dirStack.top();
    if (!afk_setCWD(newDir.c_str()))
    {
        afk_printCWDError(io_errStream);
        return false;
    }

    dirStack.pop();
    return true;
}

bool afk_readFileContents(
    const std::string& filename,
    char **o_buf,
    size_t *o_bufSize,
    std::ostream& io_errStream)
{
    FILE *f = nullptr;
    int length = 0;
    bool success = false;

#ifdef _WIN32
    errno_t openErr = fopen_s(&f, filename.c_str(), "rb");
    if (openErr != 0)
    {
        io_errStream << "Failed to open " << filename << ": " << afk_strerror(openErr);
        goto finished;
    }
#else
    f = fopen(filename.c_str(), "rb");
    if (!f)
    {
        io_errStream << "Failed to open " << filename << ": " << afk_strerror(errno);
        goto finished;
    }
#endif

    if (fseek(f, 0, SEEK_END) != 0)
    {
        io_errStream << "Failed to seek to end of " << filename << ": " << afk_strerror(errno);
        goto finished;
    }

    length = ftell(f);
    if (length < 0)
    {
        io_errStream << "Failed to find size of " << filename << ": " << afk_strerror(errno);
        goto finished;
    }

    *o_bufSize = (size_t)length;

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        io_errStream << "Failed to seek to beginning of " << filename << ": " << afk_strerror(errno);
        goto finished;
    }

    *o_buf = (char *) malloc(sizeof(char) * *o_bufSize);
    if (!(*o_buf))
    {
        io_errStream << "Failed to allocate " << *o_bufSize << "bytes for file " << filename;
        goto finished;
    }

    {
        char *read_pos = *o_buf;
        size_t length_left = static_cast<size_t>(length);
        size_t length_read;
        success = true;

        while (!feof(f) && length_left != 0)
        {
            length_read = fread(read_pos, 1, length_left, f);
            if (length_read == 0 && ferror(f))
            {
                io_errStream << "Failed to read from " << filename << ": " << afk_strerror(errno);
                success = false;
                break;
            }

            read_pos += length_read;
            length_left -= length_read;
        }
    }

finished:
    if (!success && *o_buf != NULL)
    {
        free(*o_buf);
        *o_buf = NULL;
    }

    if (f) fclose(f);
    return success;
}

