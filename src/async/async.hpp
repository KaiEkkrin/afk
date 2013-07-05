/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_ASYNC_H_
#define _AFK_ASYNC_ASYNC_H_

#include <vector>

#include <boost/atomic.hpp>
#include <boost/function.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/ref.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/future.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

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

    /* The number of still-busy workers.
     */
    boost::atomic<int> workersBusy;

public:
    AFK_AsyncControls(): workReady(0), quit(false), workersBusy(0) {}

    void control_workReady(unsigned int workerCount);
    void control_quit();

    /* Returns true if there is work to be done, false for quit */
    bool worker_waitForWork(void);

    /* Flags this worker as busy. */
    void worker_amBusy(void);

    /* Flags this worker as idle. */
    void worker_amIdle(void);

    /* Returns true if all workers are idle, i.e. they should
     * fall out of the work loop.
     */
    bool worker_allIdle(void);
};


#define ASYNC_QUEUE_TYPE(type) boost::lockfree::queue<type>

template<class ParameterType, class ReturnType>
void afk_asyncWorker(
    AFK_AsyncControls& controls,
    unsigned int id,
    boost::function<ReturnType (const ParameterType&, ASYNC_QUEUE_TYPE(ParameterType)&)> func,
    ASYNC_QUEUE_TYPE(ParameterType)& queue,
    boost::promise<ReturnType>& promise)
{
    while (controls.worker_waitForWork())
    {
        /* TODO Do something sane with the return values rather
         * than just accumulating the last one!
         */
        ReturnType retval;
        bool wasIdle = false;

        while (!controls.worker_allIdle())
        {
            /* Get the next parameter to call with. */
            ParameterType nextParameter;
            if (queue.pop(nextParameter))
            {
                /* I've got a parameter.  Make the call. */
                if (wasIdle) controls.worker_amBusy();
                wasIdle = false;
                retval = func(nextParameter, queue);
            }
            else
            {
                /* I have nothing to do.  Spin (there will probably
                 * be something very soon), but also register my
                 * idleness.
                 */
                if (!wasIdle) controls.worker_amIdle();
                wasIdle = true;
                boost::this_thread::yield();
            }
        }

        /* At this point, everyone has finished.
         * See TODO above: for expediency, if I'm worker
         * 0, I'll populate the return value, otherwise
         * I'll throw it away now
         */
        if (id == 0) promise.set_value(retval);
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
    boost::promise<ReturnType> promise;

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
            queue(queueSize)
    {
        initWorkers(func, concurrency);
    }

    AFK_AsyncGang(boost::function<ReturnType (const ParameterType&, ASYNC_QUEUE_TYPE(ParameterType)&)> func, size_t queueSize):
        queue(queueSize)
    {
        initWorkers(func, boost::thread::hardware_concurrency());
    }

    virtual ~AFK_AsyncGang()
    {
        for (std::vector<boost::thread*>::iterator wIt = workers.begin(); wIt != workers.end(); ++wIt)
            delete *wIt;
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
        /* Reset that promise */
        promise = boost::promise<ReturnType>();

        /* Set things off */
        controls.control_workReady(workers.size());

        /* Send back the future */
        return promise.get_future();
    }

    /* Tells everything to stop.  Call before a delete. */
    void stop(void)
    {
        controls.control_quit();
        for (std::vector<boost::thread*>::iterator wIt = workers.begin(); wIt != workers.end(); ++wIt)
            (*wIt)->join();
    }
};

/* TODO: I need a bigass test suite for the above :P */


#endif /* _AFK_ASYNC_ASYNC_H_ */

