/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_FILE_READFILE_H_
#define _AFK_FILE_READFILE_H_

#include <iostream>
#include <string>

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

#endif /* _AFK_FILE_READFILE_H_ */

