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
#include "config.hpp"

/* AFK_ComputeQueue implementation */

AFK_ComputeQueue::AFK_ComputeQueue(
    AFK_OclShim *_oclShim,
    cl_context& _ctxt,
    cl_device_id device,
    const AFK_Config *config,
    unsigned int _commandSet):
        oclShim(_oclShim), ctxt(_ctxt), blocking(config->clSyncReadWrite ? CL_TRUE : CL_FALSE), commandSet(_commandSet), k(0), kernelArgCount(0)
{
    cl_int error;
    q = oclShim->CreateCommandQueue()(
        ctxt,
        device,
        config->clOutOfOrder ? CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE : 0,
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

void AFK_ComputeQueue::copyBuffer(
    cl_mem srcBuf,
    cl_mem dstBuf,
    size_t size,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    assert(commandSet & (AFK_CQ_READ_COMMAND_SET | AFK_CQ_WRITE_COMMAND_SET));
    AFK_CLCHK(oclShim->EnqueueCopyBuffer()(
        q, srcBuf, dstBuf, 0, 0, size, preDep.getEventCount(), preDep.getEvents(), o_postDep.addEvent()))
}

void AFK_ComputeQueue::copyImage(
    cl_mem srcTex,
    cl_mem dstTex,
    const size_t origin[3],
    const size_t region[3],
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    assert(commandSet & (AFK_CQ_READ_COMMAND_SET | AFK_CQ_WRITE_COMMAND_SET));
    AFK_CLCHK(oclShim->EnqueueCopyImage()(
        q, srcTex, dstTex, origin, origin, region, preDep.getEventCount(), preDep.getEvents(), o_postDep.addEvent()))
}

void *AFK_ComputeQueue::mapBuffer(
    cl_mem buf,
    cl_map_flags flags,
    size_t size,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    assert(commandSet & AFK_CQ_HOST_COMMAND_SET);
    cl_int err;
    void *mapped = oclShim->EnqueueMapBuffer()(
        q, buf, blocking, flags, 0, size, preDep.getEventCount(), preDep.getEvents(), o_postDep.addEvent(), &err);
    AFK_HANDLE_CL_ERROR(err);
    return mapped;
}

void *AFK_ComputeQueue::mapImage(
    cl_mem tex,
    cl_map_flags flags,
    const size_t origin[3],
    const size_t region[3],
    size_t *imageRowPitch,
    size_t *imageSlicePitch,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    assert(commandSet & AFK_CQ_HOST_COMMAND_SET);
    cl_int err;
    void *mapped = oclShim->EnqueueMapImage()(
        q, tex, blocking, flags, origin, region, imageRowPitch, imageSlicePitch, preDep.getEventCount(), preDep.getEvents(), o_postDep.addEvent(), &err);
    AFK_HANDLE_CL_ERROR(err);
    return mapped;
}

void AFK_ComputeQueue::unmapObject(
    cl_mem obj,
    void *mapped,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& o_postDep)
{
    assert(commandSet & AFK_CQ_HOST_COMMAND_SET);
    cl_int err;
    oclShim->EnqueueUnmapMemObject()(
        q, obj, mapped, preDep.getEventCount(), preDep.getEvents(), o_postDep.addEvent(), &err);
    AFK_HANDLE_CL_ERROR(err);
}

#if 0
void AFK_ComputeQueue::readBuffer(
    cl_mem buf,
    size_t size,
    void *target,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_READ_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueReadBuffer()(
        q, buf, blocking, 0, size, target, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
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
        q, tex, blocking, origin, region, 0, 0, target, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}
#endif

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

#if 0
void AFK_ComputeQueue::writeBuffer(
    const void *source,
    cl_mem buf,
    size_t size,
    const AFK_ComputeDependency& preDep,
    AFK_ComputeDependency& postDep)
{
    assert(commandSet & AFK_CQ_WRITE_COMMAND_SET);
    AFK_CLCHK(oclShim->EnqueueWriteBuffer()(
        q, buf, blocking, 0, size, source, preDep.getEventCount(), preDep.getEvents(), postDep.addEvent()))
}
#endif

void AFK_ComputeQueue::finish(void)
{
    AFK_CLCHK(oclShim->Finish()(q))
}
