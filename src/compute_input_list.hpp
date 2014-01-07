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

#ifndef _AFK_COMPUTE_INPUT_LIST_H_
#define _AFK_COMPUTE_INPUT_LIST_H_

#include "afk.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <iterator>
#include <memory>

#include "compute_dependency.hpp"
#include "compute_input_list.hpp"
#include "compute_queue.hpp"
#include "computer.hpp"


/* A ComputeInputList wraps the idea of generating in the enumeration
 * threads a list of objects to be computed upon, and then, when the
 * render phase is reached, pushing all those objects to the OpenCL
 * device in one go.
 * The compute queue objects should have one of these for each of the
 * input lists they will manage, and this class assumes that it's being
 * wrapped in a Fair that ensures there is no concurrent enumeration
 * and render, by making a mirrored pair of each queue and flipping
 * at end-of-frame.
 */

/* ComputeInputList doesn't do its own synchronisation; it assumes that
 * the compute queue will do that.
 */

template<typename T>
class AFK_ComputeInputList
{
protected:
    /* I keep a host-side and a device-side buffer, for quick copies.
     * The host-side buffer will be mapped for the duration of the
     * enumeration phase, for quick writes into it.
     */
    cl_mem hostMem;
    cl_mem deviceMem;

    T *hostMapped;
    size_t count;
    size_t lastCount;
    size_t extendOffset;

    /* State tracking. */
    AFK_ComputeDependency hostMappingReady;
    AFK_ComputeDependency hostUnmappingReady;
    AFK_ComputeDependency pushFinished;
    bool doingPush;

    /* I need to retain one of these... */
    AFK_Computer *computer;

    size_t getSize(void) const { return count * sizeof(T); }
    size_t getExtendOffsetInBytes(void) const { return extendOffset * sizeof(T); }

    void waitForMapped(void)
    {
        /* Make sure we don't accidentally map and push in the same phase */
        assert(!doingPush);

        if (!hostMapped)
        {
            /* Enqueue a map command */
            hostMapped = reinterpret_cast<T*>(
                computer->getHostQueue()->mapBuffer(hostMem, CL_MAP_WRITE, getSize(), hostUnmappingReady, hostMappingReady));
        }

        hostMappingReady.waitFor();
    }

    void waitForUnmapped(void)
    {
        if (hostMapped)
        {
            /* Enqueue an unmap command */
            computer->getHostQueue()->unmapObject(hostMem, hostMapped, hostMappingReady, hostUnmappingReady);
            hostMapped = nullptr;
        }

        hostUnmappingReady.waitFor();
    }

    void resizeMem(size_t newMinCount)
    {
        size_t oldSize = getSize();

        while (count < newMinCount)
        {
            /* I'll try to grow it in a manner that doesn't just double,
            * but instead follows a kind of Fibonacci sequence --
            */
            size_t newCount = count + lastCount;
            lastCount = count;
            count = newCount;
        }

        /* Make some new space */
        cl_context ctxt = computer->getContext();
        cl_int err;
        cl_mem newHostMem = computer->oclShim.CreateBuffer()(
            ctxt,
            CL_MEM_ALLOC_HOST_PTR,
            getSize(),
            NULL,
            &err);
        AFK_HANDLE_CL_ERROR(err);

        cl_mem newDeviceMem = computer->oclShim.CreateBuffer()(
            ctxt,
            CL_MEM_READ_ONLY,
            getSize(),
            NULL,
            &err);
        AFK_HANDLE_CL_ERROR(err);

        if (hostMem)
        {
            /* Map existing and new space and copy the current data */
            waitForUnmapped();

            T *oldHostMapped = reinterpret_cast<T*>(
                computer->getHostQueue()->mapBuffer(hostMem, CL_MAP_READ, oldSize, hostUnmappingReady, hostMappingReady));

            hostMapped = reinterpret_cast<T*>(
                computer->getHostQueue()->mapBuffer(newHostMem, CL_MAP_WRITE, getSize(), hostUnmappingReady, hostMappingReady));

            hostMappingReady.waitFor();
#ifdef _WIN32
            std::copy(
                stdext::checked_array_iterator<T*>(oldHostMapped, lastCount),
                stdext::checked_array_iterator<T*>(oldHostMapped, lastCount) + lastCount,
                stdext::checked_array_iterator<T*>(hostMapped, count));
#else
            std::copy(oldHostMapped, oldHostMapped + lastCount, hostMapped);
#endif

            /* Finally delete the old space */
            AFK_ComputeDependency oldUnmapped(computer);
            computer->getHostQueue()->unmapObject(hostMem, oldHostMapped, hostMappingReady, oldUnmapped);
            AFK_CLCHK(computer->oclShim.ReleaseMemObject()(hostMem));
            AFK_CLCHK(computer->oclShim.ReleaseMemObject()(deviceMem));
        }

        hostMem = newHostMem;
        deviceMem = newDeviceMem;
    }


public:
    AFK_ComputeInputList(AFK_Computer *_computer, size_t startingCount = 64) :
        hostMem(0), deviceMem(0), hostMapped(nullptr),
        count(0), lastCount(startingCount), extendOffset(0), hostMappingReady(_computer), hostUnmappingReady(_computer), pushFinished(_computer), doingPush(false),
        computer(_computer)
    {
        resizeMem(startingCount);
    }

    virtual ~AFK_ComputeInputList()
    {
        waitForUnmapped();
        if (hostMem) AFK_CLCHK(computer->oclShim.ReleaseMemObject()(hostMem));
        if (deviceMem) AFK_CLCHK(computer->oclShim.ReleaseMemObject()(deviceMem));
    }

    /* Called from an enumeration thread when more data is available. */
    void extend(const T& element)
    {
        std::array<T, 1> elements = { element };
        extend(elements.cbegin(), elements.cend());
    }

    template<typename TIterable>
    void extend(const TIterable& start, const TIterable& end)
    {
        size_t newCount = (end - start);
        size_t requiredCount = extendOffset + newCount;
        if (requiredCount > count) resizeMem(requiredCount);

        waitForMapped();
        assert(hostMapped);

#ifdef _WIN32
        /* Satisfy MSVC array checking */
        std::copy(start, end, stdext::checked_array_iterator<T*>(hostMapped, count) + extendOffset);
#else
        std::copy(start, end, hostMapped + extendOffset);
#endif
        extendOffset += newCount;
    }

    size_t getCount(void) const { return extendOffset; }

    /* Called from the render thread to get the data to the GPU.
     * Returns the device mem object to be fed to the kernel.
     */
    cl_mem push(const AFK_ComputeDependency& preDep, AFK_ComputeDependency& o_postDep)
    {
        doingPush = true;

        /* Make sure we don't copy before being unmapped */
        hostUnmappingReady += preDep;

        /* Make sure that something got written */
        size_t extendOffsetInBytes = getExtendOffsetInBytes();
        assert(extendOffsetInBytes > 0);

        computer->getWriteQueue()->copyBuffer(
            hostMem, deviceMem, extendOffsetInBytes /* total amount written */, hostUnmappingReady, o_postDep);

        /* Equally, we need to wait for that copy to be finished before we can
        * map again, and before we flip
        */
        hostUnmappingReady += o_postDep;
        pushFinished += o_postDep;

        /* That resets the amount that was written */
        extendOffset = 0;

        return deviceMem;
    }

    bool empty(void) const { return (extendOffset == 0); }

    /* Call to wait for whatever we were doing to finish and clear
     * out the current data, ready to extend from zero again.
     */
    void clear(void)
    {
        /* Make sure all work is finished */
        waitForUnmapped();
        pushFinished.waitFor();

        /* Reset to the beginning */
        doingPush = false;
    }
};

#endif /* _AFK_COMPUTE_INPUT_LIST_H_ */
