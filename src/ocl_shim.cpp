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

#include "afk.hpp"

#include <iostream>

#include "exception.hpp"
#include "ocl_shim.hpp"

#ifdef AFK_GLX

void afk_handleDlError(const char *_file, const int _line)
{
    char *error = dlerror();
    if (error != NULL)
    {
        std::ostringstream errSS;
        errSS << "AFK_OclShim: Error " << error << " occurred at " << _file << ":" << _line << std::endl;
        throw AFK_Exception(errSS.str());
    }
}

AFK_OclShim::AFK_OclShim(const AFK_ConfigSettings& settings):
    handle(nullptr)
{
    if (settings.clLibDir.get().size() > 0)
    {
        std::stringstream ss;
        ss << settings.clLibDir.get() << "/libOpenCL.so";
        handle = dlopen(ss.str().c_str(), RTLD_LAZY);
    }
    else
    {
        handle = dlopen("libOpenCL.so", RTLD_LAZY);
    }

    AFK_HANDLE_DL_ERROR;
}

AFK_OclShim::~AFK_OclShim()
{
    if (handle) dlclose(handle);
}

#endif /* AFK_GLX */

#ifdef AFK_WGL

void afk_handleDlError(const char *_file, const int _line)
{
    DWORD lastError = GetLastError();
    if (lastError)
    {
        std::ostringstream errSS;
        errSS << "AFK_OclShim: Error " << lastError << " occurred at " << _file << ":" << _line << std::endl;
        throw AFK_Exception(errSS.str());
    }
}

AFK_OclShim::AFK_OclShim(const AFK_ConfigSettings& settings) :
handle(0)
{
    if (settings.clLibDir.get().size() > 0)
    {
        std::stringstream ss;
        ss << settings.clLibDir.get() << "\\OpenCL.dll";
        handle = LoadLibraryA(ss.str().c_str());
    }
    else
    {
        handle = LoadLibraryA("OpenCL.dll");
    }

    if (!handle) AFK_HANDLE_DL_ERROR;
}

AFK_OclShim::~AFK_OclShim()
{
    if (handle) FreeLibrary(handle);
}

#endif /* AFK_WGL */

AFK_OclPlatformExtensionShim::AFK_OclPlatformExtensionShim(cl_platform_id _platformId, AFK_OclShim *_oclShim) :
platformId(_platformId), oclShim(_oclShim)
{
}

