/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_GL_BUFFER_H_
#define _AFK_GL_BUFFER_H_

#include "afk.hpp"

#include <boost/lockfree/queue.hpp>

/* TODO DELETE ME.
 * This class is deprecated in this branch in favour of
 * "jigsaw", which will, you know, actually work.
 */

/* Makes a queue of GL buffers, pre-populated with the desired
 * amount of data (in zeroes).
 * Once you pop a buffer off the queue, you're responsible for
 * it.  Give it back to the queue when you've finished, please
 * (for re-use).
 */
class AFK_GLBufferQueue
{
protected:
    boost::lockfree::queue<GLuint> buffers;

public:
    AFK_GLBufferQueue(size_t bufSize, size_t bufCount, GLenum target, GLenum usage);
    virtual ~AFK_GLBufferQueue();

    /* Gives you a buffer from the queue. */
    GLuint pop(void);

    /* Returns a buffer to the queue. */
    void push(GLuint buf);
};

#endif /* _AFK_GL_BUFFER_H_ */

