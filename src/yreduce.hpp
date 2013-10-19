/* AFK
 * Copyright (C) 2013, Alex Holloway.
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
    AFK_Computer *computer;
    cl_kernel yReduceKernel;

    /* The result buffers */
    cl_mem buf;
    size_t bufSize;
    float *readback;
    size_t readbackSize; /* in floats */

    /* This event signals that the readback is ready. */
    cl_event readbackEvent;

public:
    AFK_YReduce(AFK_Computer *_computer);
    virtual ~AFK_YReduce();

    void compute(
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

