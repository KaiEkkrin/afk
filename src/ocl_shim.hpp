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

#ifndef _AFK_OCL_SHIM_H_
#define _AFK_OCL_SHIM_H_

#include "afk.hpp"

#include "config.hpp"

/* This module is responsible for dynamically loading the OpenCL
 * library and identifying its functions.
 */

void afk_handleDlError(const char *_file, const int _line);

#define AFK_HANDLE_DL_ERROR afk_handleDlError(__FILE__, __LINE__)

#ifdef AFK_GLX

#include <dlfcn.h>

/* In OclShim, the CL functions are wrapped by member wrappers. They
 * share the same name minus the `cl' prefix.  wrapper()(cl parameters ...)
 * works like the CL function and returns the same type.
 * Note that I need to resolve the symbol at time of use, rather than
 * when creating the shim: some of the functions I'll declare won't
 * actually exist in some OpenCL DSOs (e.g. 1.2+ functions in the Nvidia
 * OpenCL 1.1-only libOpenCL)...
 */
#define AFK_OCL_FUNC(rettype, name, ...) \
    typedef rettype (*OCLFUNC_##name)(__VA_ARGS__); \
    class st_##name \
    { \
    private: \
        void **handlePtr; \
        OCLFUNC_##name oclFunc_##name; \
    public: \
        st_##name(void **_handlePtr): \
            handlePtr(_handlePtr), oclFunc_##name(nullptr) {} \
        OCLFUNC_##name operator()(void) \
        { \
            if (!oclFunc_##name) \
            { \
                oclFunc_##name = (OCLFUNC_##name)dlsym(*handlePtr, "cl"#name); \
                AFK_HANDLE_DL_ERROR; \
            } \
            return oclFunc_##name; \
        } \
    }; \
    st_##name name = st_##name(&handle)

#endif /* AFK_GLX */

#ifdef AFK_WGL

#define AFK_OCL_FUNC(rettype, name, ...) \
    typedef rettype(*OCLFUNC_##name)(__VA_ARGS__); \
    class st_##name \
    { \
    private: \
        HMODULE *handlePtr; \
        OCLFUNC_##name oclFunc_##name; \
    public: \
        st_##name(HMODULE *_handlePtr) : \
            handlePtr(_handlePtr), oclFunc_##name(nullptr) {} \
        OCLFUNC_##name operator()(void) \
        { \
            if (!oclFunc_##name) \
            { \
                oclFunc_##name = (OCLFUNC_##name)GetProcAddress(*handlePtr, "cl"#name); \
                if (!oclFunc_##name) AFK_HANDLE_DL_ERROR; \
            } \
            return oclFunc_##name; \
        } \
    }; \
    st_##name name = st_##name(&handle)

#endif /* AFK_WGL */

class AFK_OclShim
{
protected:
#ifdef AFK_GLX
    void *handle;
#endif

#ifdef AFK_WGL
    HMODULE handle;
#endif

public:
    AFK_OclShim(const AFK_Config *config);
    virtual ~AFK_OclShim();

    /* There follow the functions in the dynamic OpenCL library --
     * add new ones as I use them.
     * (Or hopefully find a GLEW equivalent and throw this code away :) )
     */
    AFK_OCL_FUNC(cl_int, BuildProgram,
        cl_program program,
        cl_uint num_devices,
        const cl_device_id *device_list,
        const char *options,
        void (*pfn_notify)(cl_program, void *user_data),
        void *user_data);

    AFK_OCL_FUNC(cl_mem, CreateBuffer,
        cl_context context,
        cl_mem_flags flags,
        size_t size,
        void *host_ptr,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_command_queue, CreateCommandQueue,
        cl_context context,
        cl_device_id device,
        cl_command_queue_properties properties,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_context, CreateContext,
        cl_context_properties *properties,
        cl_uint num_devices,
        const cl_device_id *devices,
        void *pfn_notify(
            const char *errinfo,
            const void *private_info,
            size_t cb,
            void *user_data
        ),
        void *user_data,
        cl_int *errcode_ret);

#ifdef CL_VERSION_1_2
    AFK_OCL_FUNC(cl_mem, CreateFromGLTexture,
        cl_context context,
        cl_mem_flags flags,
        GLenum texture_target,
        GLint miplevel,
        GLuint texture,
        cl_int *errcode_ret);
#endif

    AFK_OCL_FUNC(cl_mem, CreateFromGLTexture2D,
        cl_context context,
        cl_mem_flags flags,
        GLenum texture_target,
        GLint miplevel,
        GLuint texture,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_mem, CreateFromGLTexture3D,
        cl_context context,
        cl_mem_flags flags,
        GLenum texture_target,
        GLint miplevel,
        GLuint texture,
        cl_int *errcode_ret);

#ifdef CL_VERSION_1_2
    AFK_OCL_FUNC(cl_mem, CreateImage,
        cl_context context,
        cl_mem_flags flags,
        const cl_image_format *image_format,
        const cl_image_desc *image_desc,
        void *host_ptr,
        cl_int *errcode_ret);
#endif

    AFK_OCL_FUNC(cl_mem, CreateImage2D,
        cl_context context,
        cl_mem_flags flags,
        const cl_image_format *image_format,
        size_t image_width,
        size_t image_height,
        size_t image_row_pitch,
        void *host_ptr,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_mem, CreateImage3D,
        cl_context context,
        cl_mem_flags flags,
        const cl_image_format *image_format,
        size_t image_width,
        size_t image_height,
        size_t image_depth,
        size_t image_row_pitch,
        size_t image_slice_pitch,
        void *host_ptr,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_kernel, CreateKernel,
        cl_program program,
        const char *kernel_name,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_program, CreateProgramWithSource,
        cl_context context,
        cl_uint count,
        const char **strings,
        const size_t *lengths,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_sampler, CreateSampler,
        cl_context context,
        cl_bool normalized_coords,
        cl_addressing_mode addressing_mode,
        cl_filter_mode filter_mode,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_int, EnqueueAcquireGLObjects,
        cl_command_queue command_queue,
        cl_uint num_objects,
        const cl_mem *mem_objects,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueCopyBuffer,
        cl_command_queue command_queue,
        cl_mem src_buffer,
        cl_mem dst_buffer,
        size_t src_offset,
        size_t dst_offset,
        size_t cb,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueCopyImage,
        cl_command_queue command_queue,
        cl_mem src_image,
        cl_mem dst_image,
        const size_t src_origin[3],
        const size_t dst_origin[3],
        const size_t region[3],
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(void *, EnqueueMapBuffer,
        cl_command_queue command_queue,
        cl_mem buffer,
        cl_bool blocking_map,
        cl_map_flags map_flags,
        size_t offset,
        size_t cb,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(void *, EnqueueMapImage,
        cl_command_queue command_queue,
        cl_mem image,
        cl_bool blocking_map,
        cl_map_flags map_flags,
        const size_t origin[3],
        const size_t region[3],
        size_t *image_row_pitch,
        size_t *image_slice_pitch,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event,
        cl_int *errcode_ret);

    AFK_OCL_FUNC(cl_int, EnqueueNDRangeKernel,
        cl_command_queue command_queue,
        cl_kernel kernel,
        cl_uint work_dim,
        const size_t *global_work_offset,
        const size_t *global_work_size,
        const size_t *local_work_size,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueReadBuffer,
        cl_command_queue command_queue,
        cl_mem buffer,
        cl_bool blocking_read,
        size_t offset,
        size_t cb,
        void *ptr,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueReadImage,
        cl_command_queue command_queue,
        cl_mem image,
        cl_bool blocking_read,
        const size_t origin[3],
        const size_t region[3],
        size_t row_pitch,
        size_t slice_pitch,
        void *ptr,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueReleaseGLObjects,
        cl_command_queue command_queue,
        cl_uint num_objects,
        const cl_mem *mem_objects,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueUnmapMemObject,
        cl_command_queue command_queue,
        cl_mem memobj,
        void *mapped_ptr,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, EnqueueWriteBuffer,
        cl_command_queue command_queue,
        cl_mem buffer,
        cl_bool blocking_write,
        size_t offset,
        size_t cb,
        const void *ptr,
        cl_uint num_events_in_wait_list,
        const cl_event *event_wait_list,
        cl_event *event);

    AFK_OCL_FUNC(cl_int, Finish,
        cl_command_queue q);
        
    AFK_OCL_FUNC(cl_int, GetDeviceIDs,
        cl_platform_id platform,
        cl_device_type device_type,
        cl_uint num_entries,
        cl_device_id *devices,
        cl_uint *num_devices);

    AFK_OCL_FUNC(cl_int, GetDeviceInfo,
        cl_device_id device,
        cl_device_info param_name,
        size_t param_value_size,
        void *param_value,
        size_t *param_value_size_ret);

    AFK_OCL_FUNC(cl_int, GetPlatformIDs,
        cl_uint num_entries,
        cl_platform_id *platforms,
        cl_uint *num_platforms);

    AFK_OCL_FUNC(cl_int, GetPlatformInfo,
        cl_platform_id platform,
        cl_platform_info param_name,
        size_t param_value_size,
        void *param_value,
        size_t *param_value_size_ret);

    AFK_OCL_FUNC(cl_int, GetProgramBuildInfo,
        cl_program program,
        cl_device_id device,
        cl_program_build_info param_name,
        size_t param_value_size,
        void *param_value,
        size_t *param_value_size_ret);

    AFK_OCL_FUNC(cl_int, ReleaseCommandQueue,
        cl_command_queue command_queue);

    AFK_OCL_FUNC(cl_int, ReleaseContext,
        cl_context context);

    AFK_OCL_FUNC(cl_int, ReleaseEvent,
        cl_event event);

    AFK_OCL_FUNC(cl_int, ReleaseKernel,
        cl_kernel kernel);

    AFK_OCL_FUNC(cl_int, ReleaseMemObject,
        cl_mem memobj);

    AFK_OCL_FUNC(cl_int, ReleaseProgram,
        cl_program program);

    AFK_OCL_FUNC(cl_int, ReleaseSampler,
        cl_sampler sampler);

    AFK_OCL_FUNC(cl_int, RetainEvent,
        cl_event event);

    AFK_OCL_FUNC(cl_int, SetKernelArg,
        cl_kernel kernel,
        cl_uint arg_index,
        size_t arg_size,
        const void *arg_value);

    AFK_OCL_FUNC(cl_int, WaitForEvents,
        cl_uint num_events,
        const cl_event *event_list);
};

#endif /* _AFK_OCL_SHIM_H_ */

