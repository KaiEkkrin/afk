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

#ifdef AFK_WGL

#include "afk.hpp"

#include <cassert>
#include <map>
#include <sstream>

#include "core.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "window_wgl.hpp"

/* This global will map HWNDs to AFK_WindowWgl instances, so that I can call
 * throw from the window callback function below.
 * Ick...
 */
std::map<HWND, AFK_WindowWgl *> afk_wndMap;

/* This utility handles the nasty case of keyboard input */
static void handleCharInput(WPARAM wParam, LPARAM lParam)
{
    /* The "down" or "up" status is indicated a bit obscurely in a
     * WM_CHAR message -- see http://msdn.microsoft.com/en-us/library/windows/desktop/ms646276(v=vs.85).aspx
     * I want to process this first to save myself unnecessary calls
     * to that nasty WideCharToMultiByte function
     */
    bool previouslyDown = ((lParam & (1 << 30)) != 0);
    bool keyReleased = ((lParam & (1 << 31)) != 0);
    if (previouslyDown == keyReleased)
    {
        /* Turn that wide char into a sensible string */
        const int mchrSize = 16;
        char mchr[mchrSize];

        int count = WideCharToMultiByte(
            CP_ACP,
            0,
            reinterpret_cast<LPWCH>(&wParam),
            1,
            mchr,
            mchrSize - 1,
            nullptr,
            nullptr
            );

        if (count > 0)
        {
            mchr[count] = '\0';

            if (!previouslyDown)
            {
                /* This is a "down" event. */
                afk_keyboard(std::string(mchr));
            }
            else
            {
                /* This is an "up" event. */
                afk_keyboardUp(std::string(mchr));
            }
        }
    }
}

/* The global window callback function (which will go back into AFK_WindowWgl
 * for everything, of course.)
 * TODO: Consider using DirectInput for this stuff instead.  It would
 * probably work out better...
 */
LRESULT CALLBACK afk_wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        afk_wndMap[hwnd]->windowCreated();
        return 0;

    case WM_CLOSE:
        afk_wndMap[hwnd]->windowDeleted();
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        afk_wndMap[hwnd]->windowResized(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_MOVE:
        /* This contortion to obtain the x and y from
         * http://msdn.microsoft.com/en-us/library/windows/desktop/ms632631(v=vs.85).aspx
         */
        afk_wndMap[hwnd]->windowMoved(
            (int)(short)LOWORD(lParam),
            (int)(short)HIWORD(lParam)
            );
        return 0;

    case WM_CHAR:
        handleCharInput(wParam, lParam);
        return 0;

    case WM_LBUTTONDOWN:
        afk_mouse(1);
        return 0;

    case WM_LBUTTONUP:
        afk_mouseUp(1);
        return 0;

    case WM_MBUTTONDOWN:
        afk_mouse(2);
        return 0;

    case WM_MBUTTONUP:
        afk_mouseUp(2);
        return 0;

    case WM_RBUTTONDOWN:
        afk_mouse(3);
        return 0;

    case WM_RBUTTONUP:
        afk_mouseUp(3);

    case WM_XBUTTONDOWN:
        /* Button 4 or 5. */
        afk_mouse(3 + GET_XBUTTON_WPARAM(wParam));
        return 0;

    case WM_XBUTTONUP:
        afk_mouseUp(3 + GET_XBUTTON_WPARAM(wParam));
        return 0;

    case WM_MOUSEMOVE:
        afk_wndMap[hwnd]->mouseMoved(
            (int)(short)LOWORD(lParam),
            (int)(short)HIWORD(lParam)
            );
        return 0;

    default:
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }
}

/* AFK_WindowWgl implementation */

/* Attributes for initialising the GL context. */
int glAttr[] = {
    WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
    WGL_CONTEXT_MINOR_VERSION_ARB, 0,
    WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
    WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
};

void AFK_WindowWgl::windowCreated(void)
{
    deviceContext = GetDC(hwnd);

    /* Configure the pixel format */
    PIXELFORMATDESCRIPTOR pixelFormatDescriptor = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        32,     /* cColorBits */
        0, 0, 0, 0, 0, 0,
        0,      /* cAlphaBits */
        0,      /* cAlphaShift */
        0,      /* cAccumBits */
        0, 0, 0, 0,
        24,     /* cDepthBits */
        0,      /* cStencilBits */
        0,      /* cAuxBuffers */
        PFD_MAIN_PLANE,
        0,      /* bReserved */
        0, 0, 0
    };

    int pixelFormat = ChoosePixelFormat(deviceContext, &pixelFormatDescriptor);
    if (!SetPixelFormat(deviceContext, pixelFormat, &pixelFormatDescriptor))
        throw AFK_Exception("Unable to set pixel format");

    /* Create an initial WGL context. */
    initialContext = wglCreateContext(deviceContext);
    if (!initialContext)
        throw AFK_Exception("Unable to create initial WGL context");

    if (!wglMakeCurrent(deviceContext, initialContext))
        throw AFK_Exception("Unable to make initial WGL context current");

    GLenum err = glewInit();
    if (err != GLEW_OK)
    {
        std::ostringstream ss;
        ss << "Unable to initialise GLEW: " << glewGetErrorString(err);
        throw AFK_Exception(ss.str());
    }

    renderContext = wglCreateContextAttribsARB(
        deviceContext,
        0,
        glAttr);
    if (!renderContext)
        throw AFK_Exception("Unable to create real WGL context");

    if (!wglMakeCurrent(deviceContext, renderContext))
        throw AFK_Exception("Unable to make real WGL context current");

    if (wglDeleteContext(initialContext)) initialContext = 0;
}

void AFK_WindowWgl::windowDeleted(void)
{
    if (deviceContext)
    {
        if (wglMakeCurrent(deviceContext, 0))
        {
            if (renderContext) wglDeleteContext(renderContext);
            if (initialContext) wglDeleteContext(initialContext);
        }
    }
}

void AFK_WindowWgl::windowResized(unsigned int windowWidth, unsigned int windowHeight)
{
    /* The render loop will check these on the next pass. */
    width = windowWidth;
    height = windowHeight;
}

void AFK_WindowWgl::windowMoved(int windowX, int windowY)
{
    x = windowX;
    y = windowY;
}

void AFK_WindowWgl::mouseMoved(int mouseX, int mouseY)
{
    if (pointerCaptured)
    {
        /* Work out co-ordinates relative to the middle of the window */
        int wMidX = (int)width / 2;
        int wMidY = (int)height / 2;

        int relX = mouseX - x - wMidX;
        int relY = mouseY - y - wMidY;

        if (relX != 0 || relY != 0)
        {
            /* There is a significant mouse displacement. */
            afk_motion(relX, relY);

            /* Reset to the middle of the window */
            SetCursorPos(x + wMidX, y + wMidY);
        }
    }
}

AFK_WindowWgl::AFK_WindowWgl(unsigned int windowWidth, unsigned int windowHeight, bool vsync):
    hwnd(0),
    deviceContext(0),
    initialContext(0),
    renderContext(0),
    width(windowWidth),
    height(windowHeight),
    x(0), /* TODO Try to place the window in a more friendly manner */
    y(0),
    pointerCaptured(false),
    windowClosed(false)
{
    WNDCLASSEXA windowClass;
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = afk_wndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.lpszMenuName = nullptr;
    windowClass.lpszClassName = wClassName;
    windowClass.hIconSm = LoadIcon(nullptr, IDI_WINLOGO);

    if (!RegisterClassExA(&windowClass))
    {
        std::ostringstream ss;
        ss << "Unable to register window class: " << GetLastError();
        throw AFK_Exception(ss.str());
    }

    hwnd = CreateWindowExA(
        0,
        wClassName,
        wAppName,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
        x, y, windowWidth, windowHeight,
        nullptr,
        nullptr,
        GetModuleHandle(nullptr),
        nullptr
        );

    if (!hwnd)
    {
        std::ostringstream ss;
        ss << "Unable to create window: " << GetLastError();
        throw AFK_Exception(ss.str());
    }

    /* Insert this window into the map so that the callback function knows
     * where to go back.
     * TODO: Is there a potential concurrency issue here (CREATE message
     * happening immediately on a different thread)?
     */
    afk_wndMap[hwnd] = this;

    if (!ShowWindow(hwnd, SW_SHOW))
        throw AFK_Exception("Unable to show window");

    if (!UpdateWindow(hwnd))
        throw AFK_Exception("Unable to update window");
}

AFK_WindowWgl::~AFK_WindowWgl()
{
    if (hwnd)
    {
        DestroyWindow(hwnd);
        afk_wndMap.erase(afk_wndMap.find(hwnd));
    }
}

unsigned int AFK_WindowWgl::getWindowWidth(void) const
{
    return width;
}

unsigned int AFK_WindowWgl::getWindowHeight(void) const
{
    return height;
}

void AFK_WindowWgl::shareGLContext(unsigned int threadId)
{
    throw AFK_Exception("Unimplemented");
}

void AFK_WindowWgl::releaseGLContext(unsigned int threadId)
{
    throw AFK_Exception("Unimplemented");
}

void AFK_WindowWgl::shareGLCLContext(AFK_Computer *computer)
{
    throw AFK_Exception("Unimplemented");
}

void AFK_WindowWgl::loopOnEvents(
    std::function<void(void)> idleFunc,
    std::function<void(const std::string&)> keyboardUpFunc,
    std::function<void(const std::string&)> keyboardDownFunc,
    std::function<void(unsigned int)> mouseUpFunc,
    std::function<void(unsigned int)> mouseDownFunc,
    std::function<void(int, int)> motionFunc)
{
    /* TODO: For now, I'm going to ignore those parameters.
     * Given the structure of the Windows eventing system I
     * don't think they're the right way to go.  Maybe just
     * calling the functions in the event module directly
     * is fine.  (Too much abstraction?)
     */

    /* Slurp all current events, without blocking for any. */
    while (!windowClosed)
    {
        MSG message;
        while (PeekMessageA(&message, hwnd, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                windowClosed = true;
            }
            else
            {
                TranslateMessage(&message);
                DispatchMessageA(&message);
            }
        }

        afk_idle();
    }
}

void AFK_WindowWgl::capturePointer(void)
{
    if (!pointerCaptured)
    {
        SetCapture(hwnd);
        pointerCaptured = true;
    }
}

void AFK_WindowWgl::letGoOfPointer(void)
{
    if (pointerCaptured)
    {
        BOOL result = ReleaseCapture();
        if (result == FALSE)
        {
            /* Why can this fail, anyway? */
            std::ostringstream ss;
            ss << "Failed to release pointer: " << GetLastError();
            throw AFK_Exception(ss.str());
        }

        pointerCaptured = false;
    }
}

void AFK_WindowWgl::switchToFullScreen(void)
{
    /* TODO: This is a bit involved.  I'm not going to support it quite yet. */
}

void AFK_WindowWgl::switchAwayFromFullScreen(void)
{
    /* TODO as above. */
}

void AFK_WindowWgl::swapBuffers(void)
{
    if (SwapBuffers(deviceContext) == FALSE)
    {
        /* Really shouldn't happen: */
        std::ostringstream ss;
        ss << "SwapBuffers failed: " << GetLastError();
        throw AFK_Exception(ss.str());
    }
}

bool AFK_WindowWgl::windowClosing(void) const
{
    return windowClosed;
}

void AFK_WindowWgl::closeWindow(void)
{
    windowClosed = true;
}

#endif /* AFK_WGL */
