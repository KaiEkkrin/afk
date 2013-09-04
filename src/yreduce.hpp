/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_YREDUCE_H_
#define _AFK_YREDUCE_H_

#include "afk.hpp"

#include <vector>

#include "computer.hpp"
#include "landscape_sizes.hpp"

class AFK_LandscapeTile;

/* This object manages the reduction of the y-bounds out of the y-displacement
 * texture.
 * Use one y-reduce per jigsaw.
 */
class AFK_YReduce
{
protected:
    cl_kernel yReduceKernel;

    /* The result buffers */
    cl_mem buf;
    size_t bufSize;
    float *readback;
    size_t readbackSize; /* in floats */

    /* This event signals that the readback is ready. */
    cl_event readbackEvent;

public:
    AFK_YReduce(const AFK_Computer *computer);

    virtual ~AFK_YReduce();

    void compute(
        cl_context ctxt,
        cl_command_queue q,
        unsigned int unitCount,
        cl_mem *units,
        cl_mem *jigsawYDisp,
        cl_sampler *yDispSampler,
        const AFK_LandscapeSizes& lSizes,
        cl_uint eventsInWaitList,
        const cl_event *eventWaitList,
        cl_event *o_event);

    void readBack(
        unsigned int unitCount,
        std::vector<AFK_LandscapeTile*>& landscapeTiles);
};

#endif /* _AFK_YREDUCE_H_ */

