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

#ifndef _AFK_WINDOW_H_
#define _AFK_WINDOW_H_

#include <functional>

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
     * events.  The idle function is called whenever there
     * aren't any events left to handle.
     * Exits when the window gets closed.
     */
    virtual void loopOnEvents(
        std::function<void (void)> idleFunc,
        std::function<void (const std::string&)> keyboardUpFunc,
        std::function<void (const std::string&)> keyboardDownFunc,
        std::function<void (unsigned int)> mouseUpFunc,
        std::function<void (unsigned int)> mouseDownFunc,
        std::function<void (int, int)> motionFunc) = 0;

    /* Captures and releases the pointer. */
    virtual void capturePointer(void) = 0;
    virtual void letGoOfPointer(void) = 0;

    /* Switches to and from full screen. */
    virtual void switchToFullScreen(void) = 0;
    virtual void switchAwayFromFullScreen(void) = 0;

    /* The obvious buffer swap */
    virtual void swapBuffers(void) = 0;

    /* Checks whether the window has been closed or is
     * about to be
     */
    virtual bool windowClosing(void) const = 0;
        
    virtual void closeWindow(void) = 0;
};

#endif /* _AFK_WINDOW_H_ */

