/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WINDOW_H_
#define _AFK_WINDOW_H_

#include <boost/function.hpp>

#include "computer.hpp"

/* This abstract class specifies the interface the rest of AFK has
 * onto the platform specific windowing layer.  Which manages the
 * OpenGL context, etc.
 */

class AFK_Window
{
public:
    virtual ~AFK_Window() {}

    /* Supplies up-to-date window information. */
    virtual unsigned int getWindowWidth(void) const = 0;
    virtual unsigned int getWindowHeight(void) const = 0;

    /* Shares the GL context with the current thread. */
    virtual void shareGLContext(unsigned int threadId) = 0;
    virtual void releaseGLContext(unsigned int threadId) = 0;

    /* Shares the GL context with the CL. */
    virtual void shareGLCLContext(AFK_Computer *computer) = 0;

    /* Loops on window event input.  Leave this going in one
     * thread.
     * Pass in the callback functions to use upon various
     * events.
     * Exits when the window gets closed.
     */
    virtual void loopOnEvents(
        boost::function<void (unsigned int)> keyboardUpFunc,
        boost::function<void (unsigned int)> keyboardDownFunc,
        boost::function<void (unsigned int)> mouseUpFunc,
        boost::function<void (unsigned int)> mouseDownFunc,
        boost::function<void (int, int)> motionFunc,
        boost::function<void (unsigned int, unsigned int)> windowReshapeFunc) = 0;

    /* Captures and releases the pointer. */
    virtual void capturePointer(void) = 0;
    virtual void letGoOfPointer(void) = 0;

    /* The obvious buffer swap */
    virtual void swapBuffers(void) = 0;

    /* Checks whether the window has been closed or is
     * about to be
     */
    virtual bool windowClosing(void) const = 0;
        
    virtual void toggleFullScreen(void) = 0;
    virtual void closeWindow(void) = 0;
};

#endif /* _AFK_WINDOW_H_ */

