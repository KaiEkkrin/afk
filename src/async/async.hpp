/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_ASYNC_H_
#define _AFK_ASYNC_ASYNC_H_

#include <exception>
#include <iostream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/chrono.hpp>
#include <boost/function.hpp>
#include <boost/ref.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include "work_queue.hpp"

/* Don't enable this unless you want MAXIMAL SPAM */
#define ASYNC_DEBUG_SPAM 0

#define ASYNC_WORKER_DEBUG 0

#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

#if ASYNC_DEBUG_SPAM
extern boost::mutex debugSpamMut;

#define ASYNC_DEBUG(chain) \
    { \
        boost::unique_lock<boost::mutex> guard(debugSpamMut); \
        boost::posix_time::ptime debugTime = boost::posix_time::microsec_clock::local_time(); \
        std::cout << "in thread " << boost::this_thread::get_id() << " at " << debugTime << ": "; \
        std::cout << chain << std::endl; \
    }

#define ASYNC_CONTROL_DEBUG(chain) \
    { \
        boost::unique_lock<boost::mutex> guard(debugSpamMut); \
        boost::posix_time::ptime debugTime = boost::posix_time::microsec_clock::local_time(); \
        std::cout << "control " << this << " in thread "; \
        std::cout << boost::this_thread::get_id() << " at " << debugTime << ": "; \
        std::cout << chain << std::endl; \
    }


#else
#define ASYNC_DEBUG(chain)
#define ASYNC_CONTROL_DEBUG(chain)
#endif /* ASYNC_DEBUG_SPAM */

/* Asynchronous function calling for AFK.  Passes out the function to
 * a threadpool.  Supports tail-recursive / multiply tail-recursive
 * behaviour (just have the function make another async call in the
 * same call group).
 * Call groups are entirely separate so that I can wait for one and
 * ignore (some day suspend?) another.  I hope that the OS won't mind
 * that I spin up lots of busy threads.  (Feh, finesse.)
 */


class AFK_AsyncException: public std::exception {};


/* Here are the mechanisms an AsyncGang uses to control its workers */
class AFK_AsyncControls
{
private:
    /* It's really important this thing doesn't get accidentally
     * duplicated
     */
    AFK_AsyncControls(const AFK_AsyncControls& controls): workerCount(controls.workerCount) {}
    AFK_AsyncControls& operator=(const AFK_AsyncControls& controls) { return *this; }

protected:
    const unsigned int workerCount;

    /* The workers wait on this condition variable until things are
     * primed and they're ready to go. Here's how we do it:
     * - The control thread flags a bit of `workReady' for each
     * worker it wants to run
     * - When a worker grabs the condition variable, it clears its
     * bit to indicate it got it.  It then waits on the variable
     * again until all bits become 0, indicating all workers are
     * sync'd up and ready to go
     * - After everything has started once, when work has finished,
     * all the workers will find a zero `workReady' value and just
     * block until the process is begun again.
     */
    boost::condition_variable workCond;
    boost::mutex workMut;
    size_t workReady;
    bool quit; /* Tells the workers to instead quit out entirely */

public:
    AFK_AsyncControls(const unsigned int _workerCount):
        workerCount(_workerCount), workReady(0), quit(false) {}

    void control_workReady(void);
    void control_quit(void);

    /* Returns true if there is work to be done, false for quit */
    bool worker_waitForWork(unsigned int id);
};


template<class ParameterType, class ReturnType>
void afk_asyncWorker(
    AFK_AsyncControls& controls,
    unsigned int id,
    AFK_WorkQueue<ParameterType, ReturnType>& queue,
    boost::promise<ReturnType>*& promise)
{
    while (controls.worker_waitForWork(id))
    {
        /* TODO Do something sane with the return values rather
         * than just accumulating the last one!
         */
        ReturnType retval;
        enum AFK_WorkQueueStatus status;
        bool finished = false;

        while (!finished)
        {
            status = queue.consume(id, retval);
            switch (status)
            {
            case AFK_WQ_BUSY:
                /* Keep looping. */
                break;

            case AFK_WQ_WAITING:
#if WORK_QUEUE_CONDITION_WAIT
                /* I don't expect this. */
                throw AFK_AsyncException();
#else
                /* Give way so I don't cram the CPU with busy-waits.
                 * This is really important -- the whole system chokes
                 * if I don't do it.
                 */
                boost::this_thread::yield();
#endif
                break;

            case AFK_WQ_FINISHED:
                /* Nothing left. */
                finished = true;
                break;

            default:
                /* Programming error. */
                throw AFK_AsyncException();
            }
        }

#if ASYNC_WORKER_DEBUG
        std::cout << "X";
#endif

        /* At this point, everyone has finished.
         * See TODO above: for expediency, if I'm worker
         * 0, I'll populate the return value, otherwise
         * I'll throw it away now
         */
        if (id == 0)
        {
            

#if ASYNC_DEBUG_SPAM
            ASYNC_DEBUG("busy field: " << std::hex << controls.workersBusy.load())
#endif
            promise->set_value(retval);
#if ASYNC_DEBUG_SPAM
            ASYNC_DEBUG("fulfilling promise " << std::hex << (void *)promise)
#endif
        }
    }
}


template<class ParameterType, class ReturnType>
class AFK_AsyncGang
{
protected:
    std::vector<boost::thread*> workers;
    AFK_AsyncControls controls;

    /* The queue of functions and parameters. */
    AFK_WorkQueue<ParameterType, ReturnType> queue;

    /* The promised return value. */
    boost::promise<ReturnType> *promise;

    void initWorkers(unsigned int concurrency)
    {
        for (unsigned int i = 0; i < concurrency; ++i)
        { 
            boost::thread *t = new boost::thread(
                afk_asyncWorker<ParameterType, ReturnType>,
                boost::ref(controls),
                i,
                boost::ref(queue),
                boost::ref(promise));
            workers.push_back(t);
        }
    }

public:
    AFK_AsyncGang(size_t queueSize, unsigned int concurrency):
        controls(concurrency), promise(nullptr)
    {
        /* Make sure the flags for this level of concurrency will fit
         * into a size_t
         */
        assert(concurrency < (sizeof(size_t) * 8));

        initWorkers(concurrency);
    }

    AFK_AsyncGang(size_t queueSize):
        controls(boost::thread::hardware_concurrency()), promise(nullptr)
    {
        initWorkers(boost::thread::hardware_concurrency() + 1);
    }

    virtual ~AFK_AsyncGang()
    {
        controls.control_quit();
        for (auto w : workers) w->join();
        for (auto w : workers) delete w;

        if (promise) delete promise;
    }

    size_t getConcurrency(void) const
    {
        return workers.size();
    }

    bool assertNoQueuedWork(void)
    {
        return queue.finished();
    }

    /* Push initial work items into the gang.  Call before calling
     * start().
     * Er, or while it's running, if you feel brave.  It should
     * be fine...
     */
    AFK_AsyncGang& operator<<(typename AFK_WorkQueue<ParameterType, ReturnType>::WorkItem workItem)
    {
        queue.push(workItem);
        return *this;
    }

    /* Tells the async task to do a run.  Returns the future
     * result.
     * The caller should wait on that future, no doubt.
     */
    boost::unique_future<ReturnType> start(void)
    {
#if ASYNC_DEBUG_SPAM
        ASYNC_DEBUG("deleting promise " << std::hex << (void *)promise)
#endif
        /* Reset that promise */
        if (promise) delete promise;
        promise = new boost::promise<ReturnType>();
#if ASYNC_DEBUG_SPAM
        ASYNC_DEBUG("making new promise " << std::hex << (void *)promise)
#endif

        /* Set things off */
        controls.control_workReady();

        /* Send back the future */
        return promise->get_future();
    }
};

#endif /* _AFK_ASYNC_ASYNC_H_ */

