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

class AFK_OclShim
{
protected:
    void *handle;   

public:
    AFK_OclShim(const AFK_Config *config);
    virtual ~AFK_OclShim();
};

#endif /* _AFK_OCL_SHIM_H_ */

