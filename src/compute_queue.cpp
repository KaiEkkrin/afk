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

#include "compute_queue.hpp"
#include "computer.hpp"

/* AFK_ComputeQueue implementation */

AFK_ComputeQueue::AFK_ComputeQueue(
    AFK_OclShim *_oclShim,
    cl_context& _ctxt,
    cl_device_id device,
    bool _async,
    unsigned int _commandSet):
        oclShim(_oclShim), ctxt(_ctxt), async(_async), commandSet(_commandSet), k(0), kernelArgCount(0)
{
    cl_int error;
    q = oclShim->CreateCommandQueue()(
        ctxt,
        device,
        async ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0,
        &error);
    AFK_HANDLE_CL_ERROR(error);
}

AFK_ComputeQueue::~AFK_ComputeQueue()
{
    oclShim->ReleaseCommandQueue()(q);
}

void AFK_ComputeQueue::kernel(cl_kernel _k)
{
    assert(commandSet & AFK_CQ_KERNEL_COMMAND_SET);
    assert(!k);
    k = _k;
    kernelArgCount = 0;
}

void AFK_ComputeQueue::kernelArg(
    size_t size,
    const void *arg)
{
    assert(commandSet & AFK_CQ_KERNEL_COMMAND_SET);
    assert(k);
    AFK_CLCHK(oclShim->SetKernelArg()(k, kernelArgCount++, size, arg))
}

void AFK_ComputeQueue::kernel2D(
    size_t *globalDim,
    size_t *localDim,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_KERNEL_COMMAND_SET);
    assert(k);
    AFK_CLCHK(oclShim->EnqueueNDRangeKernel()(
        q, k, 2, nullptr, globalDim, localDim, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
    k = 0;
}

void AFK_ComputeQueue::kernel3D(
    size_t *globalDim,
    size_t *localDim,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_KERNEL_COMMAND_SET);
    assert(k);
    AFK_CLCHK(oclShim->EnqueueNDRangeKernel()(
        q, k, 3, nullptr, globalDim, localDim, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
    k = 0;
}

void AFK_ComputeQueue::acquireGlObjects(
    cl_mem *obj,
    cl_uint count,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_READ_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueAcquireGLObjects()(
        q, count, obj, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}

void AFK_ComputeQueue::readBuffer(
    cl_mem buf,
    size_t size,
    void *target,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_READ_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueReadBuffer()(
        q, buf, async ? CL_FALSE : CL_TRUE, 0, size, target, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}

void AFK_ComputeQueue::readImage(
    cl_mem tex,
    size_t *origin,
    size_t *region,
    void *target,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_READ_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueReadImage()(
        q, tex, async ? CL_FALSE : CL_TRUE, origin, region, 0, 0, target, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}

void AFK_ComputeQueue::releaseGlObjects(
    cl_mem *obj,
    cl_uint count,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_READ_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueReleaseGLObjects()(
        q, count, obj, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}

void AFK_ComputeQueue::writeBuffer(
    const void *source,
    cl_mem buf,
    size_t size,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_WRITE_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueWriteBuffer()(
        q, buf, async ? CL_FALSE : CL_TRUE, 0, size, source, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}

cl_mem AFK_ComputeQueue::newReadOnlyBuffer(
    void *source,
    size_t size,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    cl_int error;
    cl_mem buf = oclShim->CreateBuffer()(
        ctxt,
        CL_MEM_READ_ONLY,
        size,
        nullptr,
        &error);
    AFK_HANDLE_CL_ERROR(error);

    if (source) writeBuffer(source, buf, size, preDep, postDep);
    return buf;
}

void AFK_ComputeQueue::finish(void)
{
    AFK_CLCHK(oclShim->Finish()(q))
}
