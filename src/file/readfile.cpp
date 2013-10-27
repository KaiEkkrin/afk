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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <stack>

/* The directory stack. */
std::stack<std::string> dirStack;

bool afk_pushDir(const std::string& path, std::ostream& io_errStream)
{
    /* We switch into `path', first pushing the current directory
     * onto the stack.
     */
    char *currentDir = get_current_dir_name();
    std::string currentDirStr(currentDir);
    if (chdir(path.c_str()) == -1)
    {
        io_errStream << strerror(errno);
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
    if (chdir(newDir.c_str()) == -1)
    {
        io_errStream << strerror(errno);
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
    FILE *f = NULL;
    int length = 0;
    bool success = false;

    f = fopen(filename.c_str(), "rb");
    if (!f)
    {
        io_errStream << "Failed to open " << filename << ": " << strerror(errno);
        goto finished;
    }

    if (fseek(f, 0, SEEK_END) != 0)
    {
        io_errStream << "Failed to seek to end of " << filename << ": " << strerror(errno);
        goto finished;
    }

    length = ftell(f);
    if (length < 0)
    {
        io_errStream << "Failed to find size of " << filename << ": " << strerror(errno);
        goto finished;
    }

    *o_bufSize = (size_t)length;

    if (fseek(f, 0, SEEK_SET) != 0)
    {
        io_errStream << "Failed to seek to beginning of " << filename << ": " << strerror(errno);
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
        int length_left = length;
        int length_read;
        success = true;

        while (!feof(f) && length_left > 0)
        {
            length_read = fread(read_pos, 1, length_left, f);
            if (length_read == 0 && ferror(f))
            {
                io_errStream << "Failed to read from " << filename << ": " << strerror(errno);
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

