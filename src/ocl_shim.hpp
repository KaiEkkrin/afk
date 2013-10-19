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

#include <dlfcn.h>

#include "config.hpp"

/* This module is responsible for dynamically loading the OpenCL
 * library and identifying its functions.
 */

void afk_handleDlError(const char *_file, const int _line);

#define AFK_HANDLE_DL_ERROR afk_handleDlError(__FILE__, __LINE__)

/* In OclShim, the CL functions are wrapped by member wrappers. They
 * share the same name minus the `cl' prefix.  wrapper()(cl parameters ...)
 * works like the CL function and returns the same type.
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
                if (!oclFunc_##name) AFK_HANDLE_DL_ERROR; \
            } \
            return oclFunc_##name; \
        } \
    }; \
    st_##name name = st_##name(&handle)

class AFK_OclShim
{
protected:
    void *handle;   

public:
    AFK_OclShim(const AFK_Config *config);
    virtual ~AFK_OclShim();

    /* There follow the functions in the dynamic OpenCL library --
     * add new ones as I use them.
     * (Or hopefully find a GLEW equivalent and throw this code away :) )
     */
    AFK_OCL_FUNC(cl_int, GetPlatformIDs,
        cl_uint num_entries,
        cl_platform_id *platforms,
        cl_uint *num_platforms);
};

#endif /* _AFK_OCL_SHIM_H_ */

