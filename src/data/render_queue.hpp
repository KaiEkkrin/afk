/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_DATA_RENDER_QUEUE_H_
#define _AFK_DATA_RENDER_QUEUE_H_

#include <boost/lockfree/queue.hpp>

/* This defines a render queue for AFK.
 * It should be really simple -- just push objects that need
 * to be rendered and pop them when ready -- I want to wrap
 * it in its own class like this so that I can do analysis for
 * duplicates, etc.
 */

template<typename Renderable>
class AFK_RenderQueue
{
protected:
    /* TODO: In a moment, I want to change this so that it's
     * maintaining two queues, one for enqueuement while the
     * previous frame is being popped to render.  With
     * swapping upon frame-swap.
     * But for now this is okay.
     */
    boost::lockfree::queue<Renderable> q;

public:
    AFK_RenderQueue(unsigned int qSize): q(qSize) {}
    virtual ~AFK_RenderQueue() {}

    virtual void push(Renderable& r)
    {
        q.push(r);
    }

    virtual bool pop(Renderable& r)
    {
        return q.pop(r);
    }
};

#endif /* _AFK_DATA_RENDER_QUEUE_H_ */

