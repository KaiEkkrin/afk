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

#ifndef _AFK_WINDOW_GLX_H_
#define _AFK_WINDOW_GLX_H_

#ifdef AFK_GLX

#include "afk.hpp"

#include <map>
#include <mutex>

#include "window.hpp"

/* The GLX implementation of the AFK window. */

class AFK_WindowGlx: public AFK_Window
{
protected:
    /* Information about the X display and window. */
    Display *dpy;
    XVisualInfo *visInfo;
    Window w;
    XWindowAttributes xwa;
    Atom wmState;
    Atom wmFullscreen;
    Atom wmDeleteWindow;
    bool pointerCaptured;
    bool windowClosed;

    /* The pointer events that I mask when grabbing the pointer. */
    unsigned int pointerEventMask;

    /* ...and the key events I'm interested in... */
    unsigned int keyEventMask;

    /* GLX configuration. */
    GLXContext realGlxCtx;
    GLXFBConfig *glxFbConfig;
    int numFbConfigElements;

    std::map<unsigned int, GLXContext> shadowGlxContexts;
    std::mutex shadowCtxMut;

public:
    AFK_WindowGlx(unsigned int windowWidth, unsigned int windowHeight, bool vsync);
    virtual ~AFK_WindowGlx();

    virtual unsigned int getWindowWidth(void) const;
    virtual unsigned int getWindowHeight(void) const;

    virtual void shareGLContext(unsigned int threadId);
    virtual void releaseGLContext(unsigned int threadId);
    virtual void shareGLCLContext(AFK_Computer *computer);
    virtual void loopOnEvents(
        std::function<void (void)> idleFunc,
        std::function<void (const std::string&)> keyboardUpFunc,
        std::function<void (const std::string&)> keyboardDownFunc,
        std::function<void (unsigned int)> mouseUpFunc,
        std::function<void (unsigned int)> mouseDownFunc,
        std::function<void (int, int)> motionFunc);
    virtual void capturePointer(void);
    virtual void letGoOfPointer(void);
    virtual void switchToFullScreen(void);
    virtual void switchAwayFromFullScreen(void);
    virtual void swapBuffers(void);
    virtual bool windowClosing(void) const;
    virtual void closeWindow(void);
};

#endif /* AFK_GLX */
#endif /* _AFK_WINDOW_GLX_H_ */

