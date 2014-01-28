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

/* Everything should include this file first; it makes sure that
 * "tricky" includes are pulled in in the right order.
 * Don't include anything in here anywhere else.
 */

#ifndef _AFK_AFK_H_
#define _AFK_AFK_H_

/* TODO Include OpenCL/cl.h instead on Mac (if I ever do a port) */
#include <CL/cl.h>
#include <GL/glew.h>
#include <CL/cl_gl.h>
#include <CL/cl_gl_ext.h>

#ifdef AFK_GLX
#include <X11/Xlib.h>
#include <GL/glxew.h>
#endif /* AFK_GLX */

#ifdef AFK_WGL
#include <GL/wglew.h>
#endif /* AFK_WGL */

#include <cstdint>


/* Global compile-time settings */

/* Work around some compiler differences... */

#ifndef afk_align
#ifdef __GNUC__
#define afk_align(v) __attribute__((aligned(v)))
#endif
#ifdef _WIN32
#define afk_align(v) __declspec(align(v))
#endif
#endif /* afk_align */

#ifndef afk_strdup
#ifdef _WIN32
#define afk_strdup _strdup
#else
#define afk_strdup strdup
#endif
#endif /* afk_strdup */

#ifndef afk_thread_local
#ifdef __GNUC__
#define afk_thread_local thread_local
#endif
#ifdef _WIN32
#define afk_thread_local __declspec(thread)
#endif
#endif /* afk_thread_local */

#ifdef _WIN32
/* If I don't do this, Visual Studio doesn't give me mathematical constants like M_PI */
#define _USE_MATH_DEFINES

/* TODO: This is naughty, but I don't want to spend time removing innocent strcpy() calls right now! */
#define _CRT_SECURE_NO_WARNINGS
#endif

/* This appears to be the correct amount to `wiggle' texture samples
 * by, on both AMD and Nvidia, so that nearest-neighbour sampling
 * actually picks up the correct neighbour.
 * TODO: I think I should move this wiggle into the shaders, to be
 * applied to nearest-neighbour samplers only.
 */
#define AFK_SAMPLE_WIGGLE 0.5f

/* This is a debug setting that makes AFK print information about
 * each shape that it's rendering, so that I can track problems in
 * the shape enumeration...
 */
#define AFK_SHAPE_ENUM_DEBUG 0

#endif /* _AFK_AFK_H_ */
