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

#ifndef _AFK_DREDUCE_H_
#define _AFK_DREDUCE_H_

#include "afk.hpp"

#include "compute_dependency.hpp"
#include "computer.hpp"
#include "core.hpp"
#include "keyed_cell.hpp"
#include "shape_sizes.hpp"

class AFK_ShapeCell;

/* This object is like yreduce, but manages the reduction of the
 * density bounds out of the vapour.
 * Like the rest of the vapour stuff right now, the dreduce kernel
 * takes all 4 vapours at once, so there should be one single
 * dreduce.
 */
class AFK_DReduce
{
protected:
    AFK_Computer *computer;
    cl_kernel dReduceKernel;

    /* Results */
    cl_mem buf;
    size_t bufSize;
    float *readback;
    size_t readbackSize;
    AFK_ComputeDependency readbackDep;

public:
    AFK_DReduce(AFK_Computer *_computer);
    virtual ~AFK_DReduce();

    void compute(
        size_t unitCount,
        cl_mem *units,
        const Vec2<int>& fake3D_size,
        int fake3D_mult,
        cl_mem *vapourJigsawsDensityMem, /* all 4 */
        const AFK_ShapeSizes& sSizes,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& o_postDep);

    void readBack(
        unsigned int threadId,
        size_t unitCount,
        const std::vector<AFK_KeyedCell>& shapeCells,
        AFK_SHAPE_CELL_CACHE *cache);
};

#endif /* _AFK_DREDUCE_H_ */

