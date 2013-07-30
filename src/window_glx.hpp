/* AFK (c) Alex Holloway 2013 */

#ifndef _AFK_WINDOW_GLX_H_
#define _AFK_WINDOW_GLX_H_

#include "afk.hpp"

#include <map>

#include <boost/thread/mutex.hpp>

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
    boost::mutex shadowCtxMut;

public:
    AFK_WindowGlx(unsigned int windowWidth, unsigned int windowHeight);
    virtual ~AFK_WindowGlx();

    virtual unsigned int getWindowWidth(void) const;
    virtual unsigned int getWindowHeight(void) const;

    virtual void shareGLContext(unsigned int threadId);
    virtual void releaseGLContext(unsigned int threadId);
    virtual void shareGLCLContext(AFK_Computer *computer);
    virtual void loopOnEvents(
        boost::function<void (unsigned int)> keyboardUpFunc,
        boost::function<void (unsigned int)> keyboardDownFunc,
        boost::function<void (unsigned int)> mouseUpFunc,
        boost::function<void (unsigned int)> mouseDownFunc,
        boost::function<void (int, int)> motionFunc);
    virtual void capturePointer(void);
    virtual void letGoOfPointer(void);
    virtual void toggleFullScreen(void);
    virtual void swapBuffers(void);
    virtual bool windowClosing(void) const;
    virtual void closeWindow(void);
};

#ifdef AFK_GLX

#endif /* AFK_GLX */
#endif /* _AFK_WINDOW_GLX_H_ */

