// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "stdhdrs.h"

#include "MouseSimulator.h"

#include "resource.h"

HINSTANCE CursorColorManager::hInst = nullptr;
CursorColorManager* CursorColorManager::instance = nullptr;

CursorColorManager::CursorColorManager() {}

CursorColorManager::~CursorColorManager()
{
    for (int i{ 1 }; i < 9; ++i)
    {
        ::DestroyCursor(hCorsor[static_cast<unsigned long long>(i)]);
    }
}

void CursorColorManager::Init(const HINSTANCE hInstIn)
{
    if (!this->hInst)
    {
        this->hInst = hInstIn;
        hCorsor[0U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSORBLANK));
        hCorsor[1U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSOR1));
        hCorsor[2U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSOR2));
        hCorsor[3U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSOR3));
        hCorsor[4U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSOR4));
        hCorsor[5U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSOR6));
        hCorsor[6U] = ::LoadCursor(hInstIn, MAKEINTRESOURCE(IDC_CURSOR7));
        //hCorsor[7] = LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR8));
        //hCorsor[8] = LoadCursor(hInst, MAKEINTRESOURCE(IDC_CURSOR9));
        for (int i{ 0 }; i < 7; ++i)
        {
            usedCursor[static_cast<unsigned long long>(i)] = false;
        }
    }
}

HICON CursorColorManager::getCursor()
{
    for (int i{ 1 }; i < 7; ++i)
        if (usedCursor[static_cast<unsigned long long>(i)] == false)
        {
            usedCursor[static_cast<unsigned long long>(i)] = true;
            return hCorsor[static_cast<unsigned long long>(i)];
        }
    return nullptr;
}

void CursorColorManager::releaseCursor(const HICON icon)
{
    for (int i{ 1 }; i < 7; ++i)
    {
        if (hCorsor[static_cast<unsigned long long>(i)] == icon)
        {
            usedCursor[static_cast<unsigned long long>(i)] = false;
        }
    }
}

HICON CursorColorManager::getEraser() { return hCorsor[0U]; }

CursorColorManager* CursorColorManager::getInstance()
{
    if (instance == nullptr)
    {
        instance = new CursorColorManager();
    }

    return instance;
}

SimulateCursor::SimulateCursor(const HINSTANCE hInst)
{
    this->hInst = hInst;
    CursorColorManager::getInstance()->Init(reinterpret_cast<HINSTANCE>(hInst));
    hIconMouse = CursorColorManager::getInstance()->getCursor();
    hIconErase = CursorColorManager::getInstance()->getEraser();
    DWORD dwTId;
    ::CreateThread(nullptr, 0, Start, this, 0, &dwTId);
}

SimulateCursor::~SimulateCursor()
{
    ::SendMessage(hWnd, WM_CLOSE, 0, 0);
    CursorColorManager::getInstance()->releaseCursor(hIconMouse);
}

DWORD WINAPI SimulateCursor::Start(const LPVOID lpParam)
{
    SimulateCursor* sc = reinterpret_cast<SimulateCursor*>(lpParam);
    HDESK desktop;
    desktop = ::OpenInputDesktop(0, FALSE,
                               DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    if (desktop)
    {
        ::SetThreadDesktop(desktop);
    }

    sc->hWnd = create_window(sc);
    MSG msg;
    while (::GetMessage(&msg, nullptr, 0, 0) != 0)
    {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    ::SetThreadDesktop(old_desktop);
    if (desktop)
    {
        ::CloseDesktop(desktop);
    }

    return 0;
}

HWND SimulateCursor::create_window(SimulateCursor* const simulateCursor)
{
    SimulateCursor* sc = simulateCursor;
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = reinterpret_cast<HINSTANCE>(sc->hInst);
    wcex.hIcon = nullptr;
    wcex.hCursor = nullptr;
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(::GetStockObject(HOLLOW_BRUSH));
    wcex.lpszMenuName = nullptr;
    
    wcex.lpszClassName = "mouseSimulator";
    wcex.hIconSm = nullptr;
    ::RegisterClassEx(&wcex);
    HWND hWndDesktop = ::GetDesktopWindow();
    RECT rcClient;
    if (!::GetClientRect(hWndDesktop, &rcClient))
    {
        return nullptr;
    }

    HWND hWnd = ::CreateWindowEx(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "mouseSimulator",
                               "mouseSimulator", WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_BORDER, rcClient.left,
                               rcClient.top, rcClient.right - rcClient.left, rcClient.bottom - rcClient.top, hWndDesktop, nullptr,
                               reinterpret_cast<HINSTANCE>(sc->hInst), nullptr);

#if !defined(_WIN64)
    ::SetWindowLong(hWnd, GWL_USERDATA, static_cast<long>(reinterpret_cast<INT_PTR>(sc)));
#else
    ::SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(sc));
#endif

#ifndef _X64
    LONG style = ::GetWindowLong(hWnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    ::SetWindowLong(hWnd, GWL_STYLE, style);
#else
    LONG_PTR style = ::GetWindowLongPtr(hWnd, GWL_STYLE);
    style = ::GetWindowLongPtr(hWnd, GWL_STYLE);
    style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
    ::SetWindowLongPtr(hWnd, GWL_STYLE, style);
#endif

#ifndef _X64
    LONG lExStyle = ::GetWindowLong(hWnd, GWL_EXSTYLE);
    lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    ::SetWindowLong(hWnd, GWL_EXSTYLE, lExStyle);
#else
    LONG_PTR lExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
    lExStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    ::SetWindowLongPtr(hWnd, GWL_EXSTYLE, lExStyle);
#endif

    ::ShowWindow(hWnd, true);
    ::UpdateWindow(hWnd);
    ::SetLayeredWindowAttributes(hWnd, RGB(255, 255, 255), 0, LWA_COLORKEY);
    return hWnd;
}

void SimulateCursor::moveCursor([[maybe_unused]] const int xIn, [[maybe_unused]] const int yIn)
{
    /*this->x = x;
    this->y = y;
    RECT rect;
    rect.left = x; rect.top = y;
    rect.right = x + 32; rect.bottom = y + 32;
    RECT rect2;
    rect2.left = oldx; rect2.top = oldy;
    rect2.right = oldx + 32; rect2.bottom = oldy + 32;
    HRGN rgn = CreateRectRgn(rect.left, rect.top, rect.right, rect.bottom);*/

    ::InvalidateRect(hWnd, nullptr, true);
}

LRESULT CALLBACK SimulateCursor::WndProc(const HWND hWnd, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    SimulateCursor* me;
#if !defined(_WIN64)
    me = reinterpret_cast<SimulateCursor*>(::GetWindowLong(hWnd, GWL_USERDATA));
#else
    me = reinterpret_cast<SimulateCursor*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
#endif
    if (me)
    {
        return me->realWndProc(hWnd, message, wParam, lParam);
    }
    return ::DefWindowProc(hWnd, message, wParam, lParam);
}



LRESULT CALLBACK SimulateCursor::realWndProc(const HWND hWndIn, const UINT message, const WPARAM wParam, const LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        return ::DefWindowProc(hWndIn, message, wParam, lParam);
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(hWndIn, &ps);
        if (drawn && ((x != oldx) || (y != oldy)))
        {
            ::DrawIcon(hdc, oldx, oldy, hIconErase);
        }
        ::DrawIcon(hdc, x, y, hIconMouse);
        oldx = x, oldy = y;
        drawn = true;
        ::EndPaint(hWndIn, &ps);
    }
    break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    default:
        return ::DefWindowProc(hWndIn, message, wParam, lParam);
    }
    return 0;
}
