/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORK_H_
#define _AFK_WORK_H_

#include "afk.hpp"

#include <boost/atomic.hpp>

#include "async/work_queue.hpp"
#include "def.hpp"
#include "keyed_cell.hpp"

/* Defines the work item for the world generator gang.
 * New functions that are valid in the gang need to
 * add their possible parameters to the union here.
 */


/* Some forward declarations. */
class AFK_Camera;
class AFK_Entity;
class AFK_World;

union AFK_WorldWorkParam;


/* A dependent work item is one that will float around at the
 * back of some other work items until they're done, and then
 * get enqueued by means of check().
 */
template<typename ParameterType, typename ReturnType>
class AFK_WorkDependency
{
public:
    typedef typename AFK_WorkQueue<ParameterType, ReturnType>::WorkItem WorkItem;

protected:
    boost::atomic<unsigned int> count;
    WorkItem finalItem;

public:
    AFK_WorkDependency(const WorkItem& _finalItem):
        finalItem(_finalItem)
    {
        count.store(0);
    }

    void retain(void)
    {
        count.fetch_add(1);
    }

    void retain(unsigned int times)
    {
        count.fetch_add(times);
    }

	/* Checks whether this dependency has been fulfilled.
     * If so, enqueues the final item and returns true (making
     * you responsible for deleting the object).
     * Else, returns false.
     */
    bool check(AFK_WorkQueue<ParameterType, ReturnType>& queue)
    {
        if (count.fetch_sub(1) == 1)
        {
            /* I just subtracted the last dependent task.
             * Enqueue the final task.
             */
            queue.push(finalItem);
            return true;
        }

        return false;
    }
};


/* The work parameter type is a union of all possible
 * ones :
 */
union AFK_WorldWorkParam
{
    typedef AFK_WorkDependency<union AFK_WorldWorkParam, bool> Dependency;

    struct World
    {
        AFK_Cell cell;
        AFK_World *world;
        Vec3<float> viewerLocation;
        const AFK_Camera *camera;
        unsigned int flags;
        Dependency *dependency;
    } world;

    struct Shape
    {
        AFK_KeyedCell cell;
        AFK_Entity *entity;
        AFK_World *world;
        Vec3<float> viewerLocation;
        const AFK_Camera *camera;
        unsigned int flags;
        Dependency *dependency;
    } shape;
};

typedef AFK_WorkQueue<union AFK_WorldWorkParam, bool> AFK_WorldWorkQueue;

#endif /* _AFK_WORK_H_ */

