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
#include "display.hpp"
#include "event.hpp"
#include "exception.hpp"
#include "window_wgl.hpp"

/* This global will map HWNDs to AFK_WindowWgl instances, so that I can call
 * throw from the window callback function below.
 * Ick...
 */
std::map<HWND, AFK_WindowWgl *> afk_wndMap;

/* Turns the lParam input of WM_CHAR, WM_KEYUP, WM_KEYDOWN into a scan code */
static int convertScancode(LPARAM lParam)
{
    return (((int)lParam >> 16) & ((1 << 8) - 1));
}

/* Keyboard input helper */
static std::string convertInputChar(WPARAM wParam)
{
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
        return std::string(mchr);
    }
    else
    {
        return "";
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
#if 0
    case WM_CREATE:
        afk_wndMap[hwnd]->windowCreated();
        return 0;
#endif

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
        /* Convert that scan code and cache it, then interpret it as a "key down"
        */
    {
        int scancode = convertScancode(lParam);
        afk_wndMap[hwnd]->keyTranslated(scancode, convertInputChar(wParam));
        afk_wndMap[hwnd]->keyDown(scancode);
    }
        return 0;

    case WM_KEYDOWN:
        afk_wndMap[hwnd]->keyDown(convertScancode(lParam));
        return 0;

    case WM_KEYUP:
        afk_wndMap[hwnd]->keyUp(convertScancode(lParam));
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
        return 0;

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

    case WM_CAPTURECHANGED:
        afk_wndMap[hwnd]->letGoOfPointer();
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
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SUPPORT_COMPOSITION,
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
        AFK_GLCHK("initial WGL context create");

    if (!wglMakeCurrent(deviceContext, initialContext))
        AFK_GLCHK("make initial WGL context current");

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
        AFK_GLCHK("render WGL context create");

    if (!wglMakeCurrent(deviceContext, renderContext))
        AFK_GLCHK("make render WGL context current");

    if (wglDeleteContext(initialContext)) initialContext = 0;

    std::cout << "AFK_WindowWgl: Initialised WGL render context" << std::endl;
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

void AFK_WindowWgl::keyTranslated(int scancode, const std::string& key)
{
    keycodes[scancode] = key;
}

void AFK_WindowWgl::keyDown(int scancode)
{
    auto keyIt = keycodes.find(scancode);
    if (keyIt != keycodes.end())
    {
        afk_keyboard(keyIt->second);
    }
}

void AFK_WindowWgl::keyUp(int scancode)
{
    auto keyIt = keycodes.find(scancode);
    if (keyIt != keycodes.end())
    {
        afk_keyboardUp(keyIt->second);
    }
}

void AFK_WindowWgl::mouseMoved(int mouseX, int mouseY)
{
    if (pointerCaptured)
    {
        AFK_DEBUG_PRINTL("mouse at (" << mouseX << ", " << mouseY << ") from (" << lastMouseX << ", " << lastMouseY << ")")

        /* Work out the mouse displacement. */
        int dispX = lastMouseX - mouseX;
        int dispY = lastMouseY - mouseY;

        if (dispX != 0 || dispY != 0)
        {
            afk_motion(dispX, -dispY);

            lastMouseX = mouseX;
            lastMouseY = mouseY;
        }
    }
}

void AFK_WindowWgl::warpPointer(void)
{
    assert(pointerCaptured);

    POINT middle{ width / 2, height / 2 };
    BOOL success = ClientToScreen(hwnd, &middle);
    if (success == TRUE)
        success = SetCursorPos(middle.x, middle.y);
    if (success == TRUE)
    {
        lastMouseX = width / 2;
        lastMouseY = height / 2;
    }
    else
    {
        std::ostringstream ss;
        ss << "Failed to move pointer: " << GetLastError();
        throw AFK_Exception(ss.str());
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
    lastMouseX(0),
    lastMouseY(0),
    pointerCaptured(false),
    windowClosed(false)
{
    if (windowWidth == 0 || windowHeight == 0)
    {
        /* Choose some sensible values based on the screen size */
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        if (screenWidth == 0 || screenHeight == 0)
        {
            std::ostringstream ss;
            ss << "Failed to get screen dimensions: " << GetLastError();
            throw AFK_Exception(ss.str());
        }

        width = screenWidth * 3 / 4;
        height = screenHeight * 3 / 4;
    }

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
        x, y, width, height,
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
     */
    afk_wndMap[hwnd] = this;

    if (!ShowWindow(hwnd, SW_SHOW))
        throw AFK_Exception("Unable to show window");

    if (!UpdateWindow(hwnd))
        throw AFK_Exception("Unable to update window");

    /* TODO: I don't seem to ever receive a WM_CREATE message.  I'm just going to
     * initialise that damn WGL thing right away...
     */
    windowCreated();
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
                /* Translate keyboard messages, if I don't yet remember the string for the scancode.
                 * TODO: Need to handle keyboard mapping changes (flush the `keycodes' structure)
                 */
                if (message.message == WM_KEYDOWN)
                {
                    int scancode = convertScancode(message.lParam);
                    if (keycodes.find(scancode) == keycodes.end())
                    {
                        TranslateMessage(&message);
                    }
                }

                DispatchMessageA(&message);
            }
        }

        if (pointerCaptured) warpPointer();

        afk_idle();
    }
}

void AFK_WindowWgl::capturePointer(void)
{
    if (!pointerCaptured)
    {
        SetCapture(hwnd);
        pointerCaptured = true;
        warpPointer();
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
