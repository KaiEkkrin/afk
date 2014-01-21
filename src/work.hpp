/* AFK
 * Copyright (C) 2013-2014, Alex Holloway.
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
template<typename ParameterType, typename ReturnType, typename ThreadLocalType>
class AFK_WorkDependency
{
public:
    typedef typename AFK_WorkQueue<ParameterType, ReturnType, ThreadLocalType>::WorkItem WorkItem;

protected:
    boost::atomic_uint_fast64_t count;
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

    void retain(uint_fast64_t times)
    {
        count.fetch_add(times);
    }

	/* Checks whether this dependency has been fulfilled.
     * If so, enqueues the final item and returns true (making
     * you responsible for deleting the object).
     * Else, returns false.
     */
    bool check(AFK_WorkQueue<ParameterType, ReturnType, ThreadLocalType>& queue)
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


/* The thread-local parameter is quite simple for now: */
struct AFK_WorldWorkThreadLocal
{
    AFK_Camera camera;
    Vec3<float> viewerLocation;
    float detailPitch;
};

/* The work parameter type is a union of all possible
 * ones :
 */
union AFK_WorldWorkParam
{
    typedef AFK_WorkDependency<union AFK_WorldWorkParam, bool, struct AFK_WorldWorkThreadLocal> Dependency;

    struct World
    {
        AFK_Cell cell;
        unsigned int flags;
        Dependency *dependency;  /* TODO: I suspect this is wrong and I should have a
                                  * separate dependency heap to query.  But I'm not
                                  * using it for much right now ...
                                  */
    } world;

    struct Shape
    {
        AFK_KeyedCell cell;
        Mat4<float> transformation;
        unsigned int flags;

#if AFK_SHAPE_ENUM_DEBUG
        AFK_Cell asedWorldCell;
        unsigned int asedCounter;
#endif

        Dependency *dependency;
    } shape;
};

typedef AFK_WorkQueue<union AFK_WorldWorkParam, bool, struct AFK_WorldWorkThreadLocal> AFK_WorldWorkQueue;

#endif /* _AFK_WORK_H_ */

