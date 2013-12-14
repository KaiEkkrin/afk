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

#ifndef _AFK_COMPUTE_INPUT_H_
#define _AFK_COMPUTE_INPUT_H_

#include "afk.hpp"

#include "compute_dependency.hpp"

class AFK_Computer;

/* A ComputeInput wraps a buffer used to feed input data into the CL,
 * persisting it between frames and tracking its size to avoid
 * re-allocations.
 */
class AFK_ComputeInput
{
protected:
    cl_mem buf;
    size_t size;

    /* We need to keep hold of this in order to be able to free the
     * buffer.
     * Supplying different ones on different calls is an error.
     */
    AFK_Computer *computer;

public:
    AFK_ComputeInput();
    virtual ~AFK_ComputeInput();

    /* Buffers data and returns the cl_mem to use.  You don't own it:
     * don't go releasing it or anything
     */
    cl_mem bufferData(const void *data, size_t _size, AFK_Computer *_computer, const AFK_ComputeDependency& preDep, AFK_ComputeDependency& postDep);
};

#endif /* _AFK_COMPUTE_INPUT_H_ */
