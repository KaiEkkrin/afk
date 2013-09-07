/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WORK_H_
#define _AFK_WORK_H_

#include "afk.hpp"

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


/* The world cell worker: */

/* This structure describes a world cell generation dependency.
 * Every time a cell generating worker gets a parameter with
 * one of these, it decrements `count'.  The worker that
 * decrements `count' to zero enqueues the final cell and
 * deletes this structure.
 */
struct AFK_WorldCellGenParam;

struct AFK_WorldCellGenDependency
{
    boost::atomic<unsigned int> count;
    struct AFK_WorldCellGenParam *finalCell;
};

/* The parameter for the cell generating worker.
 */
struct AFK_WorldCellGenParam
{
    AFK_Cell cell;
    AFK_World *world;
    Vec3<float> viewerLocation;
    const AFK_Camera *camera;
    unsigned int flags;
    struct AFK_WorldCellGenDependency *dependency;
};


/* The shape cell worker: */
struct AFK_ShapeCellGenParam
{
    AFK_Cell cell;
    AFK_Entity *entity;
    AFK_World *world;
    Vec3<float> viewerLocation;
    const AFK_Camera *camera;
    unsigned int flags;
};


/* The actual work parameter type, being a union of the
 * above:
 */
union AFK_WorldWorkParam
{
    struct AFK_ShapeCellGenParam shape;
    struct AFK_WorldCellGenParam world;
};

typedef AFK_WorkQueue<union AFK_WorldWorkParam, bool> AFK_WorldWorkQueue;

#endif /* _AFK_WORK_H_ */

