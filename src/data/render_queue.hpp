/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_RENDER_QUEUE_H_
#define _AFK_DATA_RENDER_QUEUE_H_

#include <exception>

#include <boost/lockfree/queue.hpp>

/* This defines a render queue for AFK.
 * It should be really simple -- just push objects that need
 * to be rendered and pop them when ready -- I want to wrap
 * it in its own class like this so that I can do analysis for
 * duplicates, etc.
 */

class AFK_RenderQueueException: public std::exception {};

template<typename Renderable>
class AFK_RenderQueue
{
protected:
    /* I have two queues.  At any one time, one of them is being
     * rendered and the other is being updated for the
     * next frame.
     */
    boost::lockfree::queue<Renderable> *q[2];
    unsigned int updateQueue;
    unsigned int drawQueue;

public:
    AFK_RenderQueue(unsigned int qSize): updateQueue(0), drawQueue(1)
    {
        /* Stupid initializer list.  I could do this better with C++11 :/ */
        q[0] = new boost::lockfree::queue<Renderable>(qSize);
        q[1] = new boost::lockfree::queue<Renderable>(qSize);
    }

    virtual ~AFK_RenderQueue()
    {
        delete q[0];
        delete q[1];
    }

    virtual void flipQueues(void)
    {
        /* Should be called from the main thread while everything
         * is halted (no concurrency issues).
         * At this point, for sanity, I'm going to verify that:
         * - everything in the draw queue got drawn
         */

        if (!q[drawQueue]->empty()) throw AFK_RenderQueueException();

        updateQueue = (updateQueue == 1 ? 0 : 1);
        drawQueue = (drawQueue == 1 ? 0 : 1);
    }

    virtual void update_push(Renderable& r)
    {
        q[updateQueue]->push(r);
    }

    virtual bool draw_pop(Renderable& r)
    {
        return q[drawQueue]->pop(r);
    }
};

#endif /* _AFK_DATA_RENDER_QUEUE_H_ */

