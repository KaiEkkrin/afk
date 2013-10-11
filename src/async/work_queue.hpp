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

#ifndef _AFK_ASYNC_WORK_QUEUE_H_
#define _AFK_ASYNC_WORK_QUEUE_H_

#include <exception>

#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/memory_order.hpp>

/* Define this to try using a condition variable to wait
 * for work to appear in the queue, rather than just
 * spinning on yield.
 * Note: Right now, this option appears to be strictly inferior
 * to spinning on this_thread::yield().  It incurs a large system
 * time penalty especially at short wait times, and long wait
 * times result in hitching that the AFK core interprets as
 * GPU overload.  If I remove the wait time some thread always
 * misses the notify and locks up entirely.  I don't think there's
 * any benefit to be had by thrashing on this.
 */
#define WORK_QUEUE_CONDITION_WAIT 0

#if WORK_QUEUE_CONDITION_WAIT
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#endif

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
public:
    /* An old style raw function pointer, because it seems
     * std::function<> isn't safe for use in a lockfree queue ??
     */
    typedef ReturnType (*WorkFunc)(
        unsigned int,
        const ParameterType&,
        AFK_WorkQueue<ParameterType, ReturnType>&);

    class WorkItem
    {
    public:
        WorkFunc func;
        ParameterType param;
    };

protected:
    boost::lockfree::queue<WorkItem> q;
    boost::atomic<unsigned int> count;

#if WORK_QUEUE_CONDITION_WAIT
    boost::shared_mutex waitMut;
    boost::condition_variable_any qHasWork;
#endif

public:
    AFK_WorkQueue(): q(100) /* arbitrary */, count(0) {}

    /* Consumes one item from the queue via its function.
     * If an item was consumed, fills out `retval' with the
     * return value.
     * Returns the work status.
     */
    enum AFK_WorkQueueStatus consume(
        unsigned int threadId,
        ReturnType& retval)
    {
        WorkItem nextItem;

        boost::atomic_thread_fence(boost::memory_order_seq_cst);
        if (count.load() > 0)
        {
            if (q.pop(nextItem))
            {
                retval = (*(nextItem.func))(threadId, nextItem.param, *this);
                /* TODO: It's hugely important that the following fetch_sub()
                 * doesn't get reordered with the fetch_add() inside the
                 * function call, or with the load() above.
                 * These fences have no basis in rational thought, but I put them
                 * in and haven't seen a thread fall out early for a while ... :-/
                 */
                boost::atomic_thread_fence(boost::memory_order_seq_cst);
                count.fetch_sub(1);
                boost::atomic_thread_fence(boost::memory_order_seq_cst);
                return AFK_WQ_BUSY;
            }
            else
            {
#if WORK_QUEUE_CONDITION_WAIT
                boost::shared_lock<boost::shared_mutex> waitLock(waitMut);
                boost::chrono::microseconds waitTime(2000); /* Kludge */
                if (count.load() > 0) qHasWork.wait_for(waitLock, waitTime);
                return AFK_WQ_BUSY;
#else
                return AFK_WQ_WAITING;
#endif
            }
        }
        else
        {
#if WORK_QUEUE_CONDITION_WAIT
            boost::shared_lock<boost::shared_mutex> waitLock(waitMut);
            qHasWork.notify_all();
#endif
            return AFK_WQ_FINISHED;
        }
    }

    void push(WorkItem parameter)
    {
        if (!q.push(parameter)) throw AFK_WorkQueueException(); /* TODO can that happen? */
        count.fetch_add(1);
#if WORK_QUEUE_CONDITION_WAIT
        {
            boost::shared_lock<boost::shared_mutex> waitLock(waitMut);
            qHasWork.notify_all();
        }
#else
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
#endif
    }

    bool finished(void)
    {
        boost::atomic_thread_fence(boost::memory_order_seq_cst);
        return (count.load() == 0);
    }
};

#endif /* _AFK_ASYNC_WORK_QUEUE_H_ */

