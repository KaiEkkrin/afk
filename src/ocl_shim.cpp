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

#include <sstream>

#include <dlfcn.h>

#include "ocl_shim.hpp"

AFK_OclShim::AFK_OclShim(const AFK_Config *config):
    handle(nullptr)
{
    if (config->clLibDir)
    {
        std::stringstream ss;
        ss << config->clLibDir << "/libOpenCL.so";
        handle = dlopen(ss.str().c_str());
    }
    else
    {
        handle = dlopen("libOpenCL.so";
    }

    
}

AFK_OclShim::~AFK_OclShim()
{
    if (handle) dlclose(handle);
}

