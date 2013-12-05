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

#ifndef _AFK_WINDOW_WGL_H_
#define _AFK_WINDOW_WGL_H_

#ifdef AFK_WGL

/* The WGL implementation of the AFK window. */

#include "afk.hpp"

#include <functional>

#include "window.hpp"

LRESULT CALLBACK afk_wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

class AFK_WindowWgl : public AFK_Window
{
protected:
    const char *wClassName = "AFKWClass";
    const char *wAppName = "AFK";
    HWND hwnd;

    HDC deviceContext;

    /* TODO: Am I about to run into the AMD problem with being unable to
     * make more than one context, like with the AMD GLX implementation?
     */
    HGLRC initialContext;
    HGLRC renderContext;

    /* I retain a copy of the window dimensions here. */
    unsigned int width, height;

    /* Also, its location, because mouse co-ordinates in Win32 are
     * client viewport relative, not window relative.
     */
    int x, y;

    bool pointerCaptured;
    bool windowClosed;

    /* Functions triggered by afk_wndProc */
    void windowCreated(void);
    void windowDeleted(void);
    void windowResized(unsigned int windowWidth, unsigned int windowHeight);
    void windowMoved(int windowX, int windowY);
    void mouseMoved(int mouseX, int mouseY);

public:
    /* Because of the mapping back from an HWND, no copying these around */
    AFK_WindowWgl(const AFK_WindowWgl& _other) = delete;
    AFK_WindowWgl& operator=(const AFK_WindowWgl& _other) = delete;

    AFK_WindowWgl(unsigned int windowWidth, unsigned int windowHeight, bool vsync);
    virtual ~AFK_WindowWgl();

    /* AFK_Window implementation ... */
    virtual unsigned int getWindowWidth(void) const;
    virtual unsigned int getWindowHeight(void) const;

    virtual void shareGLContext(unsigned int threadId);
    virtual void releaseGLContext(unsigned int threadId);
    virtual void shareGLCLContext(AFK_Computer *computer);
    virtual void loopOnEvents(
        std::function<void(void)> idleFunc,
        std::function<void(const std::string&)> keyboardUpFunc,
        std::function<void(const std::string&)> keyboardDownFunc,
        std::function<void(unsigned int)> mouseUpFunc,
        std::function<void(unsigned int)> mouseDownFunc,
        std::function<void(int, int)> motionFunc);
    virtual void capturePointer(void);
    virtual void letGoOfPointer(void);
    virtual void switchToFullScreen(void);
    virtual void switchAwayFromFullScreen(void);
    virtual void swapBuffers(void);
    virtual bool windowClosing(void) const;
    virtual void closeWindow(void);

    friend LRESULT CALLBACK afk_wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
};

#endif /* AFK_WGL */

#endif /* _AFK_WINDOW_WGL_H_ */
