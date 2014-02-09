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

#ifndef _AFK_COMPUTE_READBACK_H_
#define _AFK_COMPUTE_READBACK_H_

#include "afk.hpp"

#include <thread>
#include <vector>

#include "compute_dependency.hpp"
#include "compute_queue.hpp"


/* A ComputeReadback wraps a buffer used to get output data back from
 * the CL, in a similar vein to compute_input.
 */
template<typename T>
class AFK_ComputeReadback
{
protected:
    cl_mem buf;
    size_t bufSize;

    std::vector<T> readback;

    AFK_ComputeDependency *readbackDep;

    /* Retained in order to free the buffer, etc. */
    AFK_Computer *computer;

public:
    AFK_ComputeReadback():
        buf(0), bufSize(0), readbackDep(nullptr), computer(nullptr)
    {
    }

    virtual ~AFK_ComputeReadback()
    {
        if (buf) computer->oclShim.ReleaseMemObject()(buf);
        if (readbackDep)
        {
            readbackDep->waitFor();
            delete readbackDep;
        }
    }

    /* Makes sure the buffer is the right size and in a state to
     * pass to a kernel, and returns a pointer to it.
     */
    cl_mem *readyForKernel(AFK_Computer *_computer, size_t count)
    {
        assert(!computer || computer == _computer);
        assert(count > 0);
        computer = _computer;
        
        size_t requiredSize = count * sizeof(T);
        if (bufSize < requiredSize)
        {
            if (buf) AFK_CLCHK(computer->oclShim.ReleaseMemObject()(buf));

            cl_int error;
            buf = computer->oclShim.CreateBuffer()(
                computer->getContext(),
                CL_MEM_WRITE_ONLY,
                requiredSize,
                nullptr,
                &error);
            AFK_HANDLE_CL_ERROR(error);
            bufSize = requiredSize;
        }

        /* TODO: Change this to a memory mapping */
        readback.resize(count);

        if (!readbackDep) readbackDep = new AFK_ComputeDependency(computer);
        else readbackDep->waitFor();

        return &buf;
    }

    /* Call this after you've enqueued the kernel, with the
     * dependency to wait on before doing the readback.
     */
    void enqueueReadback(const AFK_ComputeDependency& preDep)
    {
        computer->getReadQueue()->readBuffer(buf, readback.size() * sizeof(T), readback.data(), preDep, *readbackDep);
    }

    /* Does the readback, and calls the supplied function for
     * each of the entries.  The callback function should be
     * (size_t index, const T& entry) -> (bool success).
     * If you set `insist' to true, readback() will keep iterating
     * over unsuccessful entries until they succeed; otherwise it
     * will return after one pass.
     */
    template<typename Function>
    bool readbackFinish(Function function, bool insist)
    {
        std::vector<bool> succeeded(readback.size(), false);
        bool allSucceeded = false;

        /* TODO: Asynchronous operation?  (What happens if `function'
         * is a closure?)
         */
        readbackDep->waitFor();

        do
        {
            allSucceeded = true; /* optimism! */
            for (size_t i = 0; i < readback.size(); ++i)
            {
                if (succeeded[i]) continue;
                succeeded[i] = function(i, readback[i]);
                allSucceeded &= succeeded[i];
            }

            if (insist && !allSucceeded) std::this_thread::yield();
        } while (insist && !allSucceeded);

        readback.clear();
        return allSucceeded;
    }
};

#endif /* _AFK_COMPUTE_READBACK_H_ */

