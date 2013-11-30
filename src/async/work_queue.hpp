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

#include <boost/lockfree/queue.hpp>

/* An async work queue encompasses the concept of repeatedly
 * queueing up work items to be fed to a worker function.
 *
 * The worker function's signature is of the form,
 * (threadId, ParameterType, queue ref) -> ReturnType .
 *
 * I no longer try to detect whether the overall task is finished
 * here: that's now done by the async worker function (see async.hpp).
 */

class AFK_WorkQueueException: public std::exception {};

enum AFK_WorkQueueStatus
{
    AFK_WQ_BUSY,        /* Means I just consumed a work item synchronously */
    AFK_WQ_WAITING      /* Means I didn't get a work item and am still waiting */
};

template<typename ParameterType, typename ReturnType, typename ThreadLocalType>
class AFK_WorkQueue
{
public:
    /* An old style raw function pointer, because it seems
     * std::function<> isn't safe for use in a lockfree queue ??
     */
    typedef ReturnType (*WorkFunc)(
        unsigned int,
        const ParameterType&,
        const ThreadLocalType&,
        AFK_WorkQueue<ParameterType, ReturnType, ThreadLocalType>&);

    class WorkItem
    {
    public:
        WorkFunc func;
        ParameterType param;
    };

protected:
    boost::lockfree::queue<WorkItem> q;

public:
    AFK_WorkQueue(): q(100) /* arbitrary */ {}

    /* Consumes one item from the queue via its function.
     * If an item was consumed, fills out `retval' with the
     * return value.
     * Returns the work status.
     */
    enum AFK_WorkQueueStatus consume(
        unsigned int threadId,
        ThreadLocalType& threadLocal,
        ReturnType& retval)
    {
        WorkItem nextItem;

        if (q.pop(nextItem))
        {
            retval = (*(nextItem.func))(threadId, nextItem.param, threadLocal, *this);
            return AFK_WQ_BUSY;
        }
        else
        {
            return AFK_WQ_WAITING;
        }
    }

    void push(WorkItem parameter)
    {
        if (!q.push(parameter)) throw AFK_WorkQueueException(); /* TODO can that happen? */
    }
};

#endif /* _AFK_ASYNC_WORK_QUEUE_H_ */

