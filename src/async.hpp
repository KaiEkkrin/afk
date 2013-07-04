/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_ASYNC_H_
#define _AFK_ASYNC_H_

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
    void control_workReady(unsigned int workerCount);
    void control_quit();

    /* Returns true if there is work to be done, false for quit */
    bool worker_waitForWork(void);

    /* Flags this worker as busy. */
    void worker_amBusy(void);

    /* Flags this worker as idle.  Returns true if all the other
     * workers have become idle too.
     */
    bool worker_amIdle(void);
};


#define ASYNC_QUEUE_TYPE boost::lockfree::queue<ParameterType>
#define ASYNC_WQUEUE_TYPE boost::reference_wrapper<boost::lockfree::queue<ParameterType> >

template<class ParameterType, class ReturnType>
void afk_asyncWorker(
    boost::reference_wrapper<AFK_AsyncControls> wControls,
    unsigned int id,
    boost::reference_wrapper<boost::function<ReturnType (const ParameterType&, ASYNC_WQUEUE_TYPE)> > wFunc,
    ASYNC_WQUEUE_TYPE wQueue,
    boost::reference_wrapper<boost::promise<ReturnType> > wPromise)
{
    AFK_AsyncControls& controls = wControls.get();
    boost::function<ReturnType (const ParameterType&, ASYNC_WQUEUE_TYPE)>& func = wFunc.get();
    ASYNC_QUEUE_TYPE& queue = wQueue().get;
    boost::promise<ReturnType>& promise = wPromise().get;

    while (controls.worker_waitForWork())
    {
        /* TODO Do something sane with the return values rather
         * than just accumulating the last one!
         */
        ReturnType retval;
        bool wasIdle = false;
        bool allIdle = false;

        while (!allIdle)
        {
            /* Get the next parameter to call with. */
            ParameterType nextParameter;
            if (queue.pop(nextParameter))
            {
                /* I've got a parameter.  Make the call. */
                if (wasIdle) controls.worker_amBusy();
                wasIdle = false;
                retval = func(nextParameter, wQueue);
            }
            else
            {
                /* I have nothing to do.  Spin (there will probably
                 * be something very soon), but also register my
                 * idleness.
                 */
                if (!wasIdle) allIdle = controls.worker_amIdle();
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
    std::vector<boost::thread> workers;
    AFK_AsyncControls controls;

    /* The queue of parameters to future calls of the function. */
    ASYNC_QUEUE_TYPE queue;

    /* The promised return value. */
    boost::promise<ReturnType> promise;

public:
    /* TODO Finesse this by setting a queue size?  Using guaranteed
     * lock-free calls?
     */
    AFK_AsyncGang(boost::function<ReturnType (const ParameterType&, ASYNC_WQUEUE_TYPE)> func)
    {
        /* Make a pile of workers. */
        for (unsigned int i = 0; i < boost::thread::hardware_concurrency(); ++i)
        {
            boost::thread t(
                boost::reference_wrapper<AFK_AsyncControls>(controls),
                i,
                boost::reference_wrapper<boost::function<ReturnType (const ParameterType&, ASYNC_WQUEUE_TYPE)> >(func),
                ASYNC_WQUEUE_TYPE(queue),
                boost::reference_wrapper<boost::promise<ReturnType> >(promise));
            workers.push_back(t);
        }
    }

    virtual ~AFK_AsyncGang()
    {
        controls.control_quit();
        for (std::vector<boost::thread>::iterator wIt = workers.begin(); wIt != workers.end(); ++wIt)
            wIt->join();
    }

    /* Tells the async task to do a run.  Returns a promise of
     * the result.
     * The caller should wait on that promise, no doubt.
     * TODO: For multiple async tasks going at once, I'm going to
     * want to select() between multiple promises, maybe using
     * boost::asio ??
     */
    boost::promise<ReturnType>& start(const ParameterType& parameter)
    {
        /* Reset that promise */
        promise = boost::promise<ReturnType>();

        /* Enqueue the first parameter and set things off */
        queue.push(parameter);
        controls.control_workReady(workers.size());

        /* Send back a reference to the promise.  Dear caller, please
         * don't mangle it
         */
        return promise;
    }
};


/* TODO: I need a bigass test suite for the above :P */


#endif /* _AFK_ASYNC_H_ */

