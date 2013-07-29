/* This is a test program for the replacement within AFK of GLUT with GLX.
 * (Yarggggg)
 */

#include <GL/glew.h>
#include <X11/Xlib.h>
#include <GL/glxew.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include <boost/thread/thread.hpp>

#include <iostream>

/* Doodahs. */
Display *dpy;
XVisualInfo *visInfo;
Window w;
GLXContext glxCtx;

int gl3Attr[] = {
    GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
    GLX_CONTEXT_MINOR_VERSION_ARB, 2,
    GLX_CONTEXT_FLAGS_ARB,
    GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
    0
};

int nElements;
GLXFBConfig *glxFbConfig;

boost::thread *glThread = NULL;

bool windowClosed = false;

#define WINDOW_WIDTH 1920
#define WINDOW_HEIGHT 1080

void glThreadProc()
{
    /* Use the GLX context
     * TODO: Looks like you need a separate context (shared)
     * per thread -- but I'm sure I can sort that out ...
     */
    GLXContext thGlxCtx = glXCreateContextAttribsARB(dpy, glxFbConfig[0], glxCtx, true, gl3Attr);
    glXMakeCurrent(dpy, w, thGlxCtx);

    while (!windowClosed)
    {
        glClearColor(
            random() / 0x1.0p32,
            random() / 0x1.0p32,
            random() / 0x1.0p32,
            1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glFlush();
        glXSwapBuffers(dpy, w);
    }

    glXDestroyContext(dpy, thGlxCtx);
    std::cout << "Leaving GL thread proc" << std::endl;
}

int main(int argc, char **argv)
{
    /* Hugely important to call first */
    XInitThreads();

    dpy = XOpenDisplay(NULL);
    assert(dpy);

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
    assert(visInfo != NULL);
 
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

    w = XCreateWindow(dpy, rootWindow, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
        visInfo->depth, InputOutput, visInfo->visual,
        CWBackPixel | CWBorderPixel | CWColormap | CWEventMask,
        &winAttr);

    /* Get it mapped onto the screen */
    const unsigned int pointerEventMask = 
        ButtonPressMask |
        ButtonReleaseMask |
        PointerMotionMask |
        ButtonMotionMask;
    const unsigned int keyEventMask =
        KeyPressMask |
        KeyReleaseMask;
    XSelectInput(dpy, w, pointerEventMask | keyEventMask | StructureNotifyMask);
    XMapWindow(dpy, w);

    /* Fix it so that I will actually receive destroy window events,
     * rather than X unceremoniously pulling the plug on me
     */
    Atom wmDeleteWindow = XInternAtom(dpy, "WM_DELETE_WINDOW", 0);
    XSetWMProtocols(dpy, w, &wmDeleteWindow, 1);

    /* Make a basic OpenGL context.
     * TODO GLEW -- sort out what extensions I want to use, and
     * all that shizzle ?
     */
    GLXContext basicGlxCtx = glXCreateContext(dpy, visInfo, NULL, true);
    glXMakeCurrent(dpy, w, basicGlxCtx);

    /* Now, initialise GLEW. */
    GLenum res = glewInit();
    assert(res == GLEW_OK);

    /* Now, make an OpenGL 3.2 context.
     * TODO Different version?  What version is AFK going to need?
     * How does this stuff sit with GLEW?
     */
    glxFbConfig = glXChooseFBConfig(dpy, screen, NULL, &nElements);
    assert(glxFbConfig != NULL);

    glxCtx = glXCreateContextAttribsARB(dpy, glxFbConfig[0], NULL, true, gl3Attr);

    /* Swap to it. */
    glXMakeCurrent(dpy, w, glxCtx);
    glXDestroyContext(dpy, basicGlxCtx);

    /* TODO:
     * - Capture the pointer (make it vanish, and allow continued relative movement)
     */
    bool pointerCaptured = false;
    XWindowAttributes xwa;

    while (!windowClosed)
    {
        XEvent e;
        XNextEvent(dpy, &e);
        switch (e.type)
        {
        case ButtonPress:
            std::cout << std::dec << "Button " << e.xbutton.button << " was pressed at " << e.xbutton.x << ", " << e.xbutton.y << std::endl;
            if (e.xbutton.button == 2)
            {
                if (pointerCaptured)
                {
                    /* Un-grab the pointer. */
                    XUngrabPointer(dpy, CurrentTime);
                    pointerCaptured = false;
                }
                else
                {
                    /* Grab the pointer. */
                    XGrabPointer(
                        dpy, w, True, pointerEventMask, GrabModeAsync, GrabModeAsync, w, None, CurrentTime);
                    pointerCaptured = true;
                }
            }
            else if (e.xbutton.button == 3) windowClosed = true;
            break;

        case ButtonRelease:
            std::cout << std::dec << "Button " << e.xbutton.button << " was released at " << e.xbutton.x << ", " << e.xbutton.y << std::endl;
            break;

        case KeyPress:
            std::cout << std::dec << "Key " << e.xkey.keycode << " was pressed at " << e.xkey.x << ", " << e.xkey.y << std::endl;
            if (e.xkey.keycode == 24) windowClosed = true; // q
            break;

        case KeyRelease:
            std::cout << std::dec << "Key " << e.xkey.keycode << " was released at " << e.xkey.x << ", " << e.xkey.y << std::endl;
            break;

        case MotionNotify:
            if (pointerCaptured)
            {
                int wMidX = xwa.width / 2;
                int wMidY = xwa.height / 2;

                if (e.xmotion.x != wMidX || e.xmotion.y != wMidY)
                {
                    std::cout << std::dec << "Captured motion: " << e.xmotion.x - wMidX << ", " << e.xmotion.y - wMidY << std::endl;

                    XWarpPointer(dpy, w, w, 0, 0, 0, 0, xwa.width / 2, xwa.height / 2);
                    XFlush(dpy);
                    fflush(stdout); /* Why is this necessary?  But it is!  Freaky!  */
                }
            }
            break;

        case ConfigureNotify:
            /* Note that it seems we *don't* need to rehash the GLX contexts here */
            std::cout << "Window configured: " << std::dec << e.xconfigure.width << "x" << e.xconfigure.height << std::endl;

            /* Update my copy of the window attributes. */
            XGetWindowAttributes(dpy, w, &xwa);
            break;

        case MapNotify:
            std::cout << "Window is mapped" << std::endl;

            /* Start the GL-updating thread. */
            assert(glThread == NULL);
            glThread = new boost::thread(glThreadProc);

            /* Get the initial window attributes. */
            XGetWindowAttributes(dpy, w, &xwa);

            break;

        case ClientMessage:
            if ((Atom)e.xclient.data.l[0] == wmDeleteWindow)
            {
                std::cout << "Window closed" << std::endl;
                windowClosed = true;
                break;
            }

        default:
            break;
        }
    }

    glThread->join();

    glXDestroyContext(dpy, glxCtx);
    XFree(visInfo);

    XCloseDisplay(dpy);

    std::cout << "Cleaned up successfully" << std::endl;
    return 0;
}

