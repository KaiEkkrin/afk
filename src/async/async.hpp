/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_ASYNC_H_
#define _AFK_ASYNC_ASYNC_H_

#include <iostream>
#include <vector>

#include <boost/atomic.hpp>
#include <boost/chrono.hpp>
#include <boost/function.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/ref.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

/* Don't enable this unless you want MAXIMAL SPAM */
#define ASYNC_DEBUG_SPAM 1

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
 *
 * Things that look useful:
 * - Boost.Thread obviously
 * - Boost.Lockfree for structures used on every function call.  The
 * queue of functions to call; the output landscape queue.
 * - a condition_variable to flip the thread pool between waiting-for-
 * a-call state, and running state
 * - Boost.Promise to return a promise of the finishing state which
 * the main thread can wait for
 *
 * TODO
 * To go along with this, I need to make various structures thread
 * safe.
 * - The landMap : make a new AFK_CellCache wrapper for this.  I'm
 * going to want to cache all mip levels in any case.
 * - The cached LandscapeCells: I think I no longer want the cell
 * cache to directly cache these (which contain calculated geometry).
 * Instead the cell cache should cache a device that contains a
 * pointer to the landscape cell (which may or may not be set up)
 * and also have room for a future pointer to an object vector.
 * In addition, this is where I put the TerrainCell that I've made;
 * computing terrain cells now involves walking up through the
 * landMap.
 * - The landQueue: This should turn into a suitably safe structure
 * (use Boost.Lockfree since it will be busy ?  I'm sure I can come
 * up with a sensible size for it)
 * 
 * To do cell cache eviction, I want to stamp each cache entry with
 * the time last seen.  Then, I can have a different thread run around
 * removing old entries (and their children!) trying to keep the cache
 * size within limits.
 *
 * Challenge: can I make the cell cache lockfree?  (Suggestion: use
 * an optimistic concurrency type approach.  Evict an entry but don't
 * delete it right away, if it was a recently used one, try putting it
 * right back and hope I didn't conflict with anyone :) )
 */


/* Here are the mechanisms an AsyncGang uses to control its workers */
class AFK_AsyncControls
{
private:
    /* It's really important this thing doesn't get accidentally
     * duplicated
     */
    AFK_AsyncControls(const AFK_AsyncControls& controls) {}
    AFK_AsyncControls& operator=(const AFK_AsyncControls& controls) { return *this; }

protected:
    /* The workers wait on this condition variable until things are
     * primed and they're ready to go. Here's how we do it:
     * - The control thread sets `workReady' to the number of
     * workers it wants to run
     * - When a worker grabs the condition variable it checks for
     * nonzero and if so, decrements the variable and gets going
     * - After everything has started once, when work has finished,
     * all the workers will find a zero `workReady' value and just
     * block until the process is begun again.
     */
    boost::condition_variable workCond;
    boost::mutex workMut;
    unsigned int workReady;
    bool quit; /* Tells the workers to instead quit out entirely */

public:
    /* Flags which workers are busy. */
    boost::atomic<size_t> workersBusy;

    AFK_AsyncControls(): workReady(0), quit(false), workersBusy(0) {}

    void control_workReady(unsigned int workerCount);
    void control_quit();

    /* Returns true if there is work to be done, false for quit */
    bool worker_waitForWork(void);
};


#define ASYNC_QUEUE_TYPE(type) boost::lockfree::queue<type>

template<class ParameterType, class ReturnType>
void afk_asyncWorker(
    AFK_AsyncControls& controls,
    unsigned int id,
    boost::function<ReturnType (const ParameterType&, ASYNC_QUEUE_TYPE(ParameterType)&)> func,
    ASYNC_QUEUE_TYPE(ParameterType)& queue,
    boost::promise<ReturnType>*& promise)
{
    /* This is my bit in the `controls.workersBusy' field. */
    size_t busyBit = (size_t)1 << id;

    /* This is a mask for said. */
    size_t busyMask = ~busyBit;

    while (controls.worker_waitForWork())
    {
        /* TODO Do something sane with the return values rather
         * than just accumulating the last one!
         */
        ReturnType retval;

        for (;;)
        {
            /* Get the next parameter to call with. */
            ParameterType nextParameter;
            if (queue.pop(nextParameter))
            {
                /* I've got a parameter.  Make the call. */
                controls.workersBusy.fetch_or(busyBit);
                retval = func(nextParameter, queue);

#if ASYNC_WORKER_DEBUG
                std::cout << ".";
#endif
            }
            else
            {
                /* I have nothing to do.  Spin (there will probably
                 * be something very soon), but also register my
                 * idleness.  If everyone else was already idle,
                 * leave the loop.
                 */
                if ((controls.workersBusy.fetch_and(busyMask) & busyMask) == 0) break;

                /* TODO: It looks like spinning on yield() causes a great deal
                 * of system time wasted.
                 * Trying:
                 * - sleep for a little while -- if I do this, I'm going to also
                 * want to push in an interrupt on a regular basis (but my
                 * frame metering stuff wants this anyway)
                 * - total spin?
                 * 
                 * TODO *2: It looks like at the "hang points", I don't stay
                 * waiting for some thread or other -- ALL the threads are
                 * spinning here rather than exiting the inner loop.  Hmm!
                 */
                /* boost::this_thread::yield(); */
                 boost::this_thread::sleep_for(boost::chrono::milliseconds(2));
#if ASYNC_WORKER_DEBUG
                std::cout << id;
#endif
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

    /* The queue of parameters to future calls of the function. */
    ASYNC_QUEUE_TYPE(ParameterType) queue;

    /* The promised return value. */
    boost::promise<ReturnType> *promise;

    void initWorkers(boost::function<ReturnType (const ParameterType&, ASYNC_QUEUE_TYPE(ParameterType)&)> func,
        unsigned int concurrency)
    {
        for (unsigned int i = 0; i < concurrency; ++i)
        { 
            boost::thread *t = new boost::thread(
                afk_asyncWorker<ParameterType, ReturnType>,
                boost::ref(controls),
                i,
                func,
                boost::ref(queue),
                boost::ref(promise));
            workers.push_back(t);
        }
    }

public:
    AFK_AsyncGang(boost::function<ReturnType (const ParameterType&, ASYNC_QUEUE_TYPE(ParameterType)&)> func,
        size_t queueSize, unsigned int concurrency):
            queue(queueSize), promise(NULL)
    {
        /* Make sure the flags for this level of concurrency will fit
         * into a size_t
         */
        assert(concurrency < (sizeof(size_t) * 8));

        initWorkers(func, concurrency);
    }

    AFK_AsyncGang(boost::function<ReturnType (const ParameterType&, ASYNC_QUEUE_TYPE(ParameterType)&)> func, size_t queueSize):
        queue(queueSize)
    {
        initWorkers(func, boost::thread::hardware_concurrency());
    }

    virtual ~AFK_AsyncGang()
    {
        controls.control_quit();
        for (std::vector<boost::thread*>::iterator wIt = workers.begin(); wIt != workers.end(); ++wIt)
            (*wIt)->join();

        for (std::vector<boost::thread*>::iterator wIt = workers.begin(); wIt != workers.end(); ++wIt)
            delete *wIt;

        if (promise) delete promise;
    }

    /* Push initial parameters into the gang.  Call before calling
     * start().
     * Err, or while it's running, if you feel brave.  It should
     * be fine...
     */
    AFK_AsyncGang& operator<<(const ParameterType& parameter)
    {
        queue.push(parameter);
        return *this;
    }

    /* Tells the async task to do a run.  Returns the future
     * result.
     * The caller should wait on that future, no doubt.
     * TODO: For multiple async tasks going at once, I'm going to
     * want to select() between multiple futures somehow ??
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
        controls.control_workReady(workers.size());

        /* Send back the future */
        return promise->get_future();
    }
};

/* TODO: I need a bigass test suite for the above :P */


#endif /* _AFK_ASYNC_ASYNC_H_ */

