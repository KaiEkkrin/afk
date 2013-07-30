/* AFK (c) Alex Holloway 2013 */

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

#ifdef AFK_GLX
#include <X11/Xlib.h>
#include <GL/glxew.h>
#endif /* AFK_GLX */

/* Global compile-time settings */


#endif /* _AFK_AFK_H_ */

