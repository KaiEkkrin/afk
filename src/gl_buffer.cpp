/* AFK (c) Alex Holloway 2013 */

#include <cstring>

#include "exception.hpp"
#include "gl_buffer.hpp"

/* AFK_GLBufferQueue implementation */

AFK_GLBufferQueue::AFK_GLBufferQueue(size_t bufSize, size_t bufCount, GLenum target, GLenum usage):
    buffers(bufCount)
{
    /* Prepare a dummy buffer to initialise each thing with */
    char *dummy = new char[bufSize];
    memset(dummy, 0, bufSize);

    GLuint *bufferNums = new GLuint[bufCount];
    glGenBuffers(bufCount, bufferNums);

    for (size_t i = 0; i < bufCount; ++i)
    {
        /* Paranoia. */
        if (bufferNums[i] == 0) throw AFK_Exception("Got zero buffer from glGenBuffers");

        glBindBuffer(target, bufferNums[i]);
        glBufferData(target, bufSize, dummy, usage);
        buffers.push(bufferNums[i]);
    }

    delete[] bufferNums;
    delete[] dummy;
}

AFK_GLBufferQueue::~AFK_GLBufferQueue()
{
    GLuint buf;
    while (buffers.pop(buf))
    {
        glDeleteBuffers(1, &buf);
    }
}

GLuint AFK_GLBufferQueue::pop(void)
{
    GLuint buf;
    if (!buffers.pop(buf))
        throw AFK_Exception("GL buffer queue ran out of buffers");
    return buf;
}

void AFK_GLBufferQueue::push(GLuint buf)
{
    /* I'm not going to reset it.  The first user ought to
     * assume that it's dirty.
     */
    buffers.push(buf);
}

