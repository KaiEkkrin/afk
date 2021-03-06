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

#ifdef AFK_GLX

#include "afk.hpp"

#include <cstdio>

#include <boost/tokenizer.hpp>

#include "exception.hpp"
#include "window_glx.hpp"


typedef GLXContext (*glXCreateContextProc)(Display*, XVisualInfo*, GLXContext, Bool);
typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
typedef GLXFBConfig* (*glXChooseFBConfigProc)(Display*, int, const int*, int*);

/* Attributes used to initialise the GLX context. */
int glAttr[] = {
    GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
    GLX_CONTEXT_MINOR_VERSION_ARB, 0,
    GLX_CONTEXT_FLAGS_ARB,          GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
    GLX_CONTEXT_PROFILE_MASK_ARB,   GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
    0
};

/* Simplistic error handling */
int afk_xErrorHandler(Display *dpy, XErrorEvent *error)
{
    char str[1024];
    std::ostringstream ss;

    XGetErrorText(dpy, error->error_code, str, sizeof(str));
    ss << "X Error occurred: " << str;
    throw AFK_Exception(ss.str());
}

/* AFK_WindowGlx implementation */

AFK_WindowGlx::AFK_WindowGlx(unsigned int windowWidth, unsigned int windowHeight, bool vsync):
    dpy(nullptr), visInfo(nullptr), pointerCaptured(false), windowClosed(false),
    pointerEventMask(ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ButtonMotionMask),
    keyEventMask(KeyPressMask | KeyReleaseMask),
    realGlxCtx(0), glxFbConfig(nullptr)
{
    XInitThreads();
    XSetErrorHandler(afk_xErrorHandler);
    dpy = XOpenDisplay(nullptr);
    if (!dpy) throw AFK_Exception("Unable to open X display");

    int screen = DefaultScreen(dpy);
    Window rootWindow = DefaultRootWindow(dpy);

    /* Make the GLX attributes */
    int glxAttr[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_DEPTH_SIZE,     24,
        0
    };

    visInfo = glXChooseVisual(dpy, screen, glxAttr);
    if (!visInfo) throw AFK_Exception("Unable to choose X visuals");
 
    /* Make the window */
    XSetWindowAttributes winAttr;
    winAttr.background_pixel = 0;
    winAttr.border_pixel = 0;
    winAttr.colormap = XCreateColormap(dpy, rootWindow, visInfo->visual, AllocNone);
    winAttr.event_mask = 
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask |
        ButtonMotionMask |
        KeyPressMask |
        KeyReleaseMask |
        StructureNotifyMask;

    /* Choose default dimensions, in case we were given zeroes */
    Screen *screenSt = DefaultScreenOfDisplay(dpy);
    if (windowWidth == 0)
        windowWidth = XWidthOfScreen(screenSt) * 3 / 4;
    if (windowHeight == 0)
        windowHeight = XHeightOfScreen(screenSt) * 3 / 4;

    w = XCreateWindow(dpy, rootWindow, 0, 0, windowWidth, windowHeight, 0,
        visInfo->depth, InputOutput, visInfo->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &winAttr);

    /* Map it onto the screen */
    XSelectInput(dpy, w, pointerEventMask | keyEventMask | StructureNotifyMask);
    XMapWindow(dpy, w);

    /* Save the fullscreen related atoms for later */
    wmState = XInternAtom(dpy, "_NET_WM_STATE", 0);
    wmFullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", 0);

    /* Fix it so that I will actually receive destroy window events,
     * rather than X unceremoniously pulling the plug on me
     */
    wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(dpy, w, &wmDeleteWindow, 1);

    /* I need to make the GLX context without GLEW, because GLEW needs
     * a valid context to load and initialise its other functions
     * (how awkward).
     * Remember that I must create the final context here and not a
     * basic one to test, because AMD doesn't cope with creating
     * more than one context (?) ...
     */
    glXCreateContextAttribsARBProc ccProc = (glXCreateContextAttribsARBProc)
        glXGetProcAddressARB((const GLubyte *)"glXCreateContextAttribsARB");
    if (!ccProc) throw AFK_Exception("Your GLX doesn't support GLX_ARB_create_context");

    glXChooseFBConfigProc fbcProc = (glXChooseFBConfigProc)
        glXGetProcAddressARB((const GLubyte *)"glXChooseFBConfig");
    if (!fbcProc) throw AFK_Exception("Your GLX doesn't have glXChooseFBConfig");

    glxFbConfig = (*fbcProc)(dpy, screen, nullptr, &numFbConfigElements);
    if (!glxFbConfig) throw AFK_Exception("Unable to get GLX framebuffer config");

    realGlxCtx = (*ccProc)(dpy, glxFbConfig[0], nullptr, true, glAttr);
    if (!realGlxCtx) throw AFK_Exception("Can't create GLX context (no OpenGL 4.0?)");
    if (!glXMakeCurrent(dpy, w, realGlxCtx)) throw AFK_Exception("Unable to make real GLX context current");

    /* Now I can set up. */
    GLenum res = glewInit();
    if (res != GLEW_OK) throw AFK_Exception("Unable to initialise GLEW");

    /* Fix the Vsync setting */
    bool haveSwapControl = false;
    std::string extstr = std::string(glXQueryExtensionsString(dpy, screen));
    boost::tokenizer<> extTok(extstr);
    for (auto ext : extTok)
    {
        if (ext == "GLX_EXT_swap_control")
        {
            haveSwapControl = true;
            break;
        }
    }

    if (haveSwapControl)
    {
        if (vsync)
            glXSwapIntervalEXT(dpy, glXGetCurrentDrawable(), 1);
        else
            glXSwapIntervalEXT(dpy, glXGetCurrentDrawable(), 0);
    }
}

AFK_WindowGlx::~AFK_WindowGlx()
{
    letGoOfPointer();

    if (dpy)
    {
        {
            std::unique_lock<std::mutex> shadowCtxLock(shadowCtxMut);
            for (auto sgc : shadowGlxContexts)
            {
                if (sgc.second != 0)
                {
                    glXDestroyContext(dpy, sgc.second);
                    sgc.second = 0;
                }
            }
        }

        if (realGlxCtx) glXDestroyContext(dpy, realGlxCtx);
        if (visInfo) XFree(visInfo);
        XCloseDisplay(dpy);
    }
}

unsigned int AFK_WindowGlx::getWindowWidth(void) const
{
    return xwa.width;
}

unsigned int AFK_WindowGlx::getWindowHeight(void) const
{
    return xwa.height;
}

void AFK_WindowGlx::shareGLContext(unsigned int threadId)
{
    std::unique_lock<std::mutex> shadowCtxLock(shadowCtxMut);

    /* Try to cache and keep around per-thread GLX contexts
     * if I can.
     * Just in case someone goes around calling them willy nilly.
     */
    if (shadowGlxContexts.find(threadId) == shadowGlxContexts.end())
    {
        shadowGlxContexts[threadId] = glXCreateContextAttribsARB(dpy, glxFbConfig[0], realGlxCtx, true, glAttr);
        if (!shadowGlxContexts[threadId]) throw AFK_Exception("Unable to create new shadow GLX context");
    }

    glXMakeCurrent(dpy, w, shadowGlxContexts[threadId]);
}

void AFK_WindowGlx::releaseGLContext(unsigned int threadId)
{
    std::unique_lock<std::mutex> shadowCtxLock(shadowCtxMut);

    auto ctxtEntry = shadowGlxContexts.find(threadId);
    if (ctxtEntry != shadowGlxContexts.end())
    {
        glXDestroyContext(dpy, ctxtEntry->second);
        shadowGlxContexts.erase(ctxtEntry);
    }
}

void AFK_WindowGlx::shareGLCLContext(AFK_Computer *computer)
{
    throw AFK_Exception("Unimplemented");
}

static std::string keycodeToString(XKeyEvent *keyEvent)
{
    const int bufLen = 16;
    char buf[bufLen];

    /* TODO: here, use the KeySym field to cope with keys that
     * aren't mapped to ASCII (e.g. function keys), and work
     * out a platform-independent way of eventing such keys.
     */
    int num = XLookupString(keyEvent, buf, bufLen - 1, nullptr, nullptr);
    if (num > 0)
    {
        buf[num] = '\0';
        return std::string(buf);
    }
    else return "";
}

void AFK_WindowGlx::loopOnEvents(
    std::function<void (void)> idleFunc,
    std::function<void (const std::string&)> keyboardUpFunc,
    std::function<void (const std::string&)> keyboardDownFunc,
    std::function<void (unsigned int)> mouseUpFunc,
    std::function<void (unsigned int)> mouseDownFunc,
    std::function<void (int, int)> motionFunc)
{
    while (!windowClosed)
    {
        while (XPending(dpy))
        {
            std::string keyStr;

            XEvent e;
            XNextEvent(dpy, &e);
            switch (e.type)
            {
            case ButtonPress:
                mouseDownFunc(e.xbutton.button);
                break;

            case ButtonRelease:
                mouseUpFunc(e.xbutton.button);
                break;

            case KeyPress:
                keyStr = keycodeToString(&e.xkey);
                if (!keyStr.empty()) keyboardDownFunc(keyStr);
                break;

            case KeyRelease:
                keyStr = keycodeToString(&e.xkey);
                if (!keyStr.empty()) keyboardUpFunc(keyStr);
                break;

            case MotionNotify:
                if (pointerCaptured)
                {
                    int wMidX = xwa.width / 2;
                    int wMidY = xwa.height / 2;
    
                    if (e.xmotion.x != wMidX || e.xmotion.y != wMidY)
                    {
                        motionFunc(wMidX - e.xmotion.x, e.xmotion.y - wMidY);
    
                        XWarpPointer(dpy, w, w, 0, 0, 0, 0, xwa.width / 2, xwa.height / 2);
                        //XFlush(dpy);
                        fflush(stdout); /* Why is this necessary?  But it is!  Freaky!  */
                    }
                }
                break;

            case ConfigureNotify:
            case MapNotify:
                /* Update my copy of the window attributes. */
                XGetWindowAttributes(dpy, w, &xwa);
                break;

            case MappingNotify:
                if (e.xmapping.request == MappingModifier ||
                    e.xmapping.request == MappingKeyboard)
                {
                    /* I need to service this to make sure that
                     * XLookupString returns up to date results.
                     */
                    XRefreshKeyboardMapping(&e.xmapping);
                }
                break;

            case ClientMessage:
                if ((Atom)e.xclient.data.l[0] == wmDeleteWindow)
                {
                    windowClosed = true;
                    break;
                }

            default:
                break;
            }
        }

        if (!windowClosed) idleFunc();
    }
}

void AFK_WindowGlx::capturePointer(void)
{
    if (!pointerCaptured)
    {
        XGrabPointer(dpy, w, True, pointerEventMask, GrabModeAsync, GrabModeAsync, w, None, CurrentTime);
        pointerCaptured = true;
    }
}

void AFK_WindowGlx::letGoOfPointer(void)
{
    if (pointerCaptured)
    {
        XUngrabPointer(dpy, CurrentTime);
        pointerCaptured = false;
    }
}

void AFK_WindowGlx::switchToFullScreen(void)
{
    XEvent e;
    e.type                  = ClientMessage;
    e.xclient.window        = w;
    e.xclient.message_type  = wmState;
    e.xclient.format        = 32;
    e.xclient.data.l[0]     = 1;
    e.xclient.data.l[1]     = wmFullscreen;
    e.xclient.data.l[2]     = 0;

    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask, &e);
}

void AFK_WindowGlx::switchAwayFromFullScreen(void)
{
    XEvent e;
    e.type                  = ClientMessage;
    e.xclient.window        = w;
    e.xclient.message_type  = wmState;
    e.xclient.format        = 32;
    e.xclient.data.l[0]     = 0;
    e.xclient.data.l[1]     = wmFullscreen;
    e.xclient.data.l[2]     = 0;

    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask, &e);
}

void AFK_WindowGlx::swapBuffers(void)
{
    glXSwapBuffers(dpy, w);
}

bool AFK_WindowGlx::windowClosing(void) const
{
    return windowClosed;
}

void AFK_WindowGlx::closeWindow(void)
{
    windowClosed = true;
}

#endif /* AFK_GLX */

