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

#include <map>
#include <sstream>

#include "exception.hpp"
#include "window_wgl.hpp"

/* This global will map HWNDs to AFK_WindowWgl instances, so that I can call
 * throw from the window callback function below.
 * Ick...
 */
std::map<HWND, AFK_WindowWgl *> afk_wndMap;

/* The global window callback function (which will go back into AFK_WindowWgl
 * for everything, of course.)
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

AFK_WindowWgl::AFK_WindowWgl(unsigned int windowWidth, unsigned int windowHeight, bool vsync):
    hwnd(0),
    deviceContext(0),
    initialContext(0),
    renderContext(0),
    width(windowWidth),
    height(windowHeight)
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
        0, 0, windowWidth, windowHeight,
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

#endif /* AFK_WGL */
