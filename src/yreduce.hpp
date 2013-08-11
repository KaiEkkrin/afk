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
 */
class AFK_YReduce
{
protected:
    cl_kernel yReduceKernel;

    /* Each jigsaw gets its associated y-reduce result buffer.
     * These need remaking if they turn out too small, but
     * hopefully that won't happen too often.
     */
    std::vector<cl_mem> bufs;
    std::vector<size_t> bufSizes; /* in bytes */

    float *readback;
    size_t readbackSize; /* in floats */

public:
    AFK_YReduce(const AFK_Computer *computer);
    virtual ~AFK_YReduce();

    void compute(
        cl_context ctxt,
        cl_command_queue q,
        unsigned int puzzle,
        unsigned int unitCount,
        cl_mem *units,
        cl_mem *jigsawYDisp,
        cl_sampler *yDispSampler,
        std::vector<AFK_LandscapeTile*> *landscapeTiles,
        AFK_LandscapeSizes& lSizes);
};

#endif /* _AFK_YREDUCE_H_ */

