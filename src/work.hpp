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
    } world;

    struct Shape
    {
        AFK_Cell cell;
        AFK_Entity *entity;
        AFK_World *world;
        Vec3<float> viewerLocation;
        const AFK_Camera *camera;
        unsigned int flags;
    } shape;
};

typedef AFK_WorkQueue<union AFK_WorldWorkParam, bool> AFK_WorldWorkQueue;

#endif /* _AFK_WORK_H_ */

