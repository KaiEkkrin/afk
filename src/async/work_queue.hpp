/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_WORK_QUEUE_H_
#define _AFK_ASYNC_WORK_QUEUE_H_

#include <exception>

#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>

/* An async work queue encompasses the concept of repeatedly
 * queueing up work items to be fed to a worker function.
 * It tracks the number of work items *either queued or being
 * worked on*, and notifies when work has run out entirely.
 *
 * The worker function's signature is of the form,
 * (threadId, ParameterType, queue ref) -> ReturnType .
 */

class AFK_WorkQueueException: public std::exception {};

enum AFK_WorkQueueStatus
{
    AFK_WQ_BUSY,        /* Means I just consumed a work item synchronously */
    AFK_WQ_WAITING,     /* Means I didn't get a work item, but work is still going on */
    AFK_WQ_FINISHED     /* Means work is finished */
};

template<typename ParameterType, typename ReturnType>
class AFK_WorkQueue
{
protected:
    boost::lockfree::queue<ParameterType> q;

    /* TODO I think I might be getting a reorder around this thing... :-( */
    volatile boost::atomic<unsigned int> count;

public:
    AFK_WorkQueue(): q(100) /* arbitrary */, count(0) {}

    /* Consumes one item from the queue via the supplied function.
     * If an item was consumed, fills out `retval' with the
     * return value.
     * Returns the work status.
     */
    enum AFK_WorkQueueStatus consume(
        unsigned int threadId,
        boost::function<ReturnType (unsigned int, const ParameterType&, AFK_WorkQueue<ParameterType, ReturnType>&)> func,
        ReturnType& retval)
    {
        ParameterType nextParameter;

        if (count.load() > 0)
        {
            if (q.pop(nextParameter))
            {
                retval = func(threadId, nextParameter, *this);
                count.fetch_sub(1);
                return AFK_WQ_BUSY;
            }
            else return AFK_WQ_WAITING;
        }
        else return AFK_WQ_FINISHED;
    }

    void push(ParameterType parameter)
    {
        if (!q.push(parameter)) throw new AFK_WorkQueueException(); /* TODO can that happen? */
        count.fetch_add(1);
    }

    bool finished(void)
    {
        return (count.load() == 0);
    }
};

#endif /* _AFK_ASYNC_WORK_QUEUE_H_ */

