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

#ifndef _AFK_COMPUTE_QUEUE_H_
#define _AFK_COMPUTE_QUEUE_H_

#include "afk.hpp"

#include "compute_dependency.hpp"
#include "ocl_shim.hpp"

/* This module wraps OpenCL command queues with conveniently shaped
 * methods.
 */

#define AFK_CQ_KERNEL_COMMAND_SET   1
#define AFK_CQ_READ_COMMAND_SET     2
#define AFK_CQ_WRITE_COMMAND_SET    4

class AFK_ComputeQueue
{
protected:
    AFK_OclShim *oclShim;
    cl_context ctxt;
    cl_command_queue q;
    bool async;

    /* To verify that I'm issuing the right set of commands to
     * various queues.
     */
    unsigned int commandSet;

    /* The next kernel to be called. */
    cl_kernel k;

    /* This tracks the number of kernel args you've pushed since the
     * last call.
     * (Don't prep several calls at once! :P )
     */
    unsigned int kernelArgCount;

public:
    AFK_ComputeQueue(
        AFK_OclShim *_oclShim,
        cl_context& _ctxt,
        cl_device_id device,
        bool _async,
        unsigned int _commandSet);
    virtual ~AFK_ComputeQueue();

    /* The "kernel" stuff is stateful.  Call kernel() to set which
     * kernel you're going to call, kernelArg() to push the arguments
     * and kernel3D() finally.
     */
    void kernel(cl_kernel _k);

    void kernelArg(
        size_t size,
        const void *arg);

    void kernel2D(
        size_t *globalDim,
        size_t *localDim,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    void kernel3D(
        size_t *globalDim,
        size_t *localDim,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    void acquireGlObjects(
        cl_mem *obj,
        cl_uint count,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    void readBuffer(
        cl_mem buf,
        size_t size,
        void *target,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    void readImage(
        cl_mem tex,
        size_t *origin,
        size_t *region,
        void *target,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    void releaseGlObjects(
        cl_mem *obj,
        cl_uint count,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    void writeBuffer(
        const void *source,
        cl_mem buf,
        size_t size,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);

    /* This makes a new buffer and writes the given source to it. */
    cl_mem newReadOnlyBuffer(
        void *source,
        size_t size,
        const AFK_ComputeDependency& preDep,
        AFK_ComputeDependency& postDep);
};

#endif /* _AFK_COMPUTE_QUEUE_H_ */

