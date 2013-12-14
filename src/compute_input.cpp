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

#include "afk.hpp"

#include <cassert>
#include <memory>

#include "compute_input.hpp"
#include "compute_queue.hpp"
#include "computer.hpp"

/* AFK_ComputeInput implementation */

AFK_ComputeInput::AFK_ComputeInput() :
buf(0), size(0), computer(nullptr)
{
}

AFK_ComputeInput::~AFK_ComputeInput()
{
    if (buf)
    {
        assert(computer);
        AFK_CLCHK(computer->oclShim.ReleaseMemObject()(buf))
    }
}

cl_mem AFK_ComputeInput::bufferData(
    const void *data,
    size_t _size,
    AFK_Computer *_computer,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(!computer || computer == _computer);
    computer = _computer;

    if (buf && size < _size)
    {
        /* I'm going to need to make a new, bigger buffer. */
        AFK_CLCHK(computer->oclShim.ReleaseMemObject()(buf))
        buf = 0;
    }

    if (!buf)
    {
        /* Make a new buffer of the required size. */
        cl_int error;
        buf = computer->oclShim.CreateBuffer()(
            computer->getContext(),
            CL_MEM_READ_WRITE,
            _size,
            nullptr,
            &error);

        AFK_HANDLE_CL_ERROR(error);

        size = _size;
    }

    /* Now push the data in. */
    computer->getWriteQueue()->writeBuffer(data, buf, _size, preDep, postDep);
    return buf;
}
