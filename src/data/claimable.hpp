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

#ifndef _AFK_DATA_CLAIMABLE_H_
#define _AFK_DATA_CLAIMABLE_H_

#include <exception>
#include <functional>

#include "frame.hpp"

/* A "Claimable" is a lockable thing with useful features. 
 * It can optionally
 * stamp the object with the frame time of use, and exclude
 * re-use until the next frame.
 *
 * This file contains just the common definitions for
 * Claimables.  Include claimable_locked or claimable_volatile,
 * depending on which you're using.
 */

#define AFK_DEBUG_CLAIMABLE 0

#if AFK_DEBUG_CLAIMABLE
#include "../debug.hpp"
#define AFK_DEBUG_PRINT_CLAIMABLE(expr) AFK_DEBUG_PRINT(expr)
#define AFK_DEBUG_PRINTL_CLAIMABLE(expr) AFK_DEBUG_PRINTL(expr)
#else
#define AFK_DEBUG_PRINT_CLAIMABLE(expr)
#define AFK_DEBUG_PRINTL_CLAIMABLE(expr)
#endif

/* This exception is thrown when you can't get something because
 * it's in use.  Try again.
 * Actual program errors are assert()ed instead.
 */
class AFK_ClaimException: public std::exception {};

#define AFK_CL_LOOP         1       /* yield loops volatile claimable; blocks locked claimable */
#define AFK_CL_SPIN         2       /* tight loops volatile claimable; blocks locked claimable */
#define AFK_CL_BLOCK        4       /* volatile claimable returns right away; blocks locked claimable */
#define AFK_CL_EXCLUSIVE    8
#define AFK_CL_EVICTOR      16
#define AFK_CL_SHARED       32
#define AFK_CL_UPGRADE      64

#define AFK_CL_IS_BLOCKING(flags) ((flags & AFK_CL_LOOP) || (flags & AFK_CL_SPIN) || (flags & AFK_CL_BLOCK))
#define AFK_CL_IS_SHARED(flags) ((flags & AFK_CL_SHARED) || (flags & AFK_CL_UPGRADE))

/* How to specify a function to obtain the current computing frame */
typedef std::function<const AFK_Frame& (void)> AFK_GetComputingFrame;

#endif /* _AFK_DATA_CLAIMABLE_H_ */

