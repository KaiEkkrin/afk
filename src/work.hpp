/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORK_H_
#define _AFK_WORK_H_

#include "afk.hpp"

#include <boost/atomic.hpp>

#include "async/work_queue.hpp"
#include "cell.hpp"
#include "def.hpp"

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
 * Initialising a WorkDependency grants it ownership of the
 * pointer.
 * TODO: This mechanism is very generic!  Perhaps move it into
 * async/work_queue, so that I don't have to treat it
 * separately in world and shape?
 */
template<typename ParameterType, typename ReturnType>
class AFK_WorkDependency
{
public:
    typedef typename AFK_WorkQueue<ParameterType, ReturnType>::WorkItem WorkItem;

protected:
    boost::atomic<unsigned int> count;
    boost::atomic<WorkItem*> finalItem;

public:
    AFK_WorkDependency(WorkItem *_finalItem)
    {
        count.store(1);
        finalItem.store(_finalItem);
    }

    void retain(void)
    {
        count.fetch_add(1);
    }

    void check(AFK_WorkQueue<ParameterType, ReturnType>& queue)
    {
        if (count.fetch_sub(1) == 1)
        {
            /* I just subtracted the last dependent task.
             * Enqueue the final task.
             */
            WorkItem *fi = finalItem.exchange(NULL);
            if (fi != NULL)
            {
                queue.push(*fi);
                delete fi;
            }
        }
    }
};


/* The work parameter type is a union of all possible
 * ones :
 */
union AFK_WorldWorkParam
{
    struct World
    {
        AFK_Cell cell;
        AFK_World *world;
        Vec3<float> viewerLocation;
        const AFK_Camera *camera;
        unsigned int flags;
        AFK_WorkDependency<union AFK_WorldWorkParam, bool> *dependency;
    } world;

    struct Shape
    {
        AFK_Cell cell;
        AFK_Entity *entity;
        AFK_World *world;
        Vec3<float> viewerLocation;
        const AFK_Camera *camera;
        unsigned int flags;
        AFK_WorkDependency<union AFK_WorldWorkParam, bool> *dependency;
    } shape;
};

typedef AFK_WorkQueue<union AFK_WorldWorkParam, bool> AFK_WorldWorkQueue;

#endif /* _AFK_WORK_H_ */

