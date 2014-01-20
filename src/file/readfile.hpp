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

#ifndef _AFK_FILE_READFILE_H_
#define _AFK_FILE_READFILE_H_

#include <iostream>
#include <string>

/* Stringifies a C runtime error. */
std::string afk_strerror(errno_t e);

/* Gets the current working directory. (Yes, this is platform specific!)
 * This function allocates memory for its return value which you need
 * to free with free() after use.
 */
char *afk_getCWD(void);

/* Sets the current working directory.  Yes, this is platform specific
 * too!
 */
bool afk_setCWD(const char *dir);

/* Prints the last error in case of the above going wrong. */
void afk_printCWDError(std::ostream& io_errStream);

/* Pushes a directory onto a stack of directories we've switched into.
 * On failure, returns false and fills out `io_errStream' with the error
 * message.
 */
bool afk_pushDir(const std::string& path, std::ostream& io_errStream);

/* Pops a directory off the stack of directories we've switched into.
 * On failure, returns false and fills out `io_errStream' with the error
 * message.
 */
bool afk_popDir(std::ostream& io_errStream);

/* These next functions are implemented using C I/O, not C++.  It looks
 * like I'm stuck with that right now: the trick for redirecting the C++
 * standard streams to a Windows console breaks if I use C++ file I/O.
 * I can change this when I make a GUI.
 */

/* Reads the entire contents of a file as binary, allocating the memory
 * to store it and returning it.
 * Fills out `o_buf' and `o_bufSize' with the results.
 * Returns true if success, else false.  On error, fills out `io_errStream'
 * with the error message.
 */
bool afk_readFileContents(
    const std::string& filename,
    char **o_buf,
    size_t *o_bufSize,
    std::ostream& io_errStream);

/* Similarly, writes a file out, overwriting whatever was already there. */
bool afk_writeFileContents(
    const std::string& filename,
    const char *buf,
    size_t bufSize,
    std::ostream& io_errStream);

#endif /* _AFK_FILE_READFILE_H_ */

