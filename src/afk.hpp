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
#include <GL/freeglut.h>

/* Global compile-time settings */


/* TODO This needs to be default when a single threaded system is detected,
 * because async hangs trying to start when only one worker thread is
 * specified.
 * OTOH -- do I just want to say "stuff those systems", load one extra
 * thread, and avoid the complexity of supporting this?
 */
#define AFK_NO_THREADING 0

#endif /* _AFK_AFK_H_ */

