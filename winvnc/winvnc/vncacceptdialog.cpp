// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// vncAcceptDialog.cpp: implementation of the vncAcceptDialog class, used
// to query whether or not to accept incoming connections.

#include "stdhdrs.h"

#include "vncacceptdialog.h"

#include "common/win32_helpers.h"
#include "Localization.h" // Act : add localization on messages
#include "resource.h"
#include "SettingsManager.h"
#include "winvnc.h"

#include <array>
#include <cstdlib>
#include <cstring>
//	[v1.0.2-jp1 fix] Load resouce from dll

extern HINSTANCE hInstResDLL;

// Constructor

vncAcceptDialog::vncAcceptDialog(UINT timeoutSecs, BOOL acceptOnTimeout, const char* ipAddress, char* const infoMsg,
                                 const bool notification, bool infoMsgIsUtf8)
{
    m_timeoutSecs = timeoutSecs;
    m_ipAddress = _strdup(ipAddress);
    m_foreground_hack = FALSE;
    m_acceptOnTimeout = acceptOnTimeout;
    ThreadHandle = nullptr;
    this->infoMsg = infoMsg;
    this->notification = notification;
    m_infoMsgIsUtf8 = infoMsgIsUtf8;
}

// Destructor

vncAcceptDialog::~vncAcceptDialog()
{
    if (m_ipAddress)
    {
        std::free(m_ipAddress);
    }
    if (ThreadHandle)
    {
        ::CloseHandle(ThreadHandle);
    }
}

// Routine called to activate the dialog and, once it's done, delete it
DWORD WINAPI DialogThread(const LPVOID lpParam)
{
    vncAcceptDialog* dialog = (vncAcceptDialog*)lpParam;
    HDESK desktop;
    desktop = ::OpenInputDesktop(0, FALSE,
                               DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    ::SetThreadDesktop(desktop);

    //	[v1.0.2-jp1 fix]
    //int retVal = DialogBoxParam(hAppInstance, MAKEINTRESOURCE(IDD_ACCEPT_CONN),
    int retVal =
        ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_ACCEPT_CONN), nullptr, (DLGPROC)dialog->vncAcceptDlgProc, (LONG_PTR)dialog);

    ::SetThreadDesktop(old_desktop);
    ::CloseDesktop(desktop);
    switch (retVal)
    {
    case IDREJECT:
        return 0;
    case IDACCEPT:
        return 1;
    }
    return (dialog->m_acceptOnTimeout) ? 1 : 0;
}

BOOL vncAcceptDialog::DoDialog()
{
    DWORD dwTId;
    ThreadHandle = ::CreateThread(nullptr, 0, DialogThread, this, 0, &dwTId);
    ::WaitForSingleObject(ThreadHandle, INFINITE);
    DWORD lpExitCode;
    ::GetExitCodeThread(ThreadHandle, &lpExitCode);
    if (lpExitCode == 1)
    {
        return 1;
    }
    return 0;
}

// Callback function - handles messages sent to the dialog box

BOOL CALLBACK vncAcceptDialog::vncAcceptDlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    // This is a static method, so we don't know which instantiation we're
    // dealing with. But we can get a pseudo-this from the parameter to
    // WM_INITDIALOG, which we therafter store with the window and retrieve
    // as follows:
    static HBITMAP hbmBkGnd = nullptr;
    vncAcceptDialog* _this = helper::SafeGetWindowUserData<vncAcceptDialog>(hwnd);
    switch (uMsg)
    {
        // Dialog has just been created
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        // Save the lParam into our user data so that subsequent calls have
        // access to the parent C++ object
        helper::SafeSetWindowUserData(hwnd, lParam);
        vncAcceptDialog* _this = (vncAcceptDialog*)lParam;

        // Set the IP-address string
        std::array<char, 512U> baseMsg;
        ::strcpy_s(baseMsg.data(), baseMsg.size(), settings->getAccept_reject_mesg());

        if (std::strlen(baseMsg.data()) == 0)
        {
            ::strcpy_s(baseMsg.data(), baseMsg.size(), "UltraVNC Server has received an incoming connection from");
        }

        // Base accept/reject message is in the system/INI code page (CP_ACP).
        // The infoMsg is either UTF-8 (new Unicode path) or CP_ACP (legacy viewer).
        std::array<wchar_t, 512U> wAcceptReject{ 0 };
        ::MultiByteToWideChar(CP_ACP, 0, baseMsg.data(), -1, wAcceptReject.data(), wAcceptReject.size());

        if (std::strlen(_this->infoMsg) > 0)
        {
            ::wcscat_s(wAcceptReject.data(), wAcceptReject.size(), L"\r\n");
            std::array<wchar_t, 256U> wInfo{ 0 };
            UINT cp = _this->m_infoMsgIsUtf8 ? CP_UTF8 : CP_ACP;
            ::MultiByteToWideChar(cp, 0, _this->infoMsg, -1, wInfo.data(), wInfo.size());
            ::wcscat_s(wAcceptReject.data(), wAcceptReject.size(), wInfo.data());
        }
        ::SetDlgItemTextW(hwnd, IDC_STATIC_TEXT1, wAcceptReject.data());

        std::array<wchar_t, 256U> wIpAddress{ 0 };
        ::MultiByteToWideChar(CP_ACP, 0, _this->m_ipAddress, -1, wIpAddress.data(), wIpAddress.size());
        ::SetDlgItemTextW(hwnd, IDC_ACCEPT_IP, wIpAddress.data());

        {
            std::array<char, MAX_PATH> mycommand;
            ::strcpy_s(mycommand.data(), mycommand.size(), winvncFolder.data());
            ::strcat_s(mycommand.data(), mycommand.size(), "\\mylogo.bmp");
            hbmBkGnd = (HBITMAP)::LoadImage(nullptr, mycommand.data(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
        }
        ::SendMessage(::GetDlgItem(hwnd, IDC_ACCEPTLOGO), STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)(HBITMAP)hbmBkGnd);

        if (::SetTimer(hwnd, 1, 1000, nullptr) == 0)
        {
            if (_this->m_acceptOnTimeout)
            {
                ::EndDialog(hwnd, IDACCEPT);
            }
            else
            {
                ::EndDialog(hwnd, IDREJECT);
            }
        }
        _this->m_timeoutCount = _this->m_timeoutSecs;

        std::array<char, 256U> temp;
        if (_this->m_acceptOnTimeout)
        {
            ::sprintf_s(temp.data(), temp.size(), "AutoAccept:%u", (_this->m_timeoutCount));
        }
        else
        {
            ::sprintf_s(temp.data(), temp.size(), "AutoReject:%u", (_this->m_timeoutCount));
        }
        ::SetDlgItemText(hwnd, IDC_ACCEPT_TIMEOUT, temp.data());

        if (!_this->m_foreground_hack)
        {
            ::SetForegroundWindow(hwnd);
        }

        // Beep
        ::MessageBeep(MB_ICONEXCLAMATION);

        // Return success!
        return TRUE;
    }

        // Timer event
    case WM_TIMER:
        if ((_this->m_timeoutCount) == 0)
        {
            if (_this->m_acceptOnTimeout)
            {
                ::EndDialog(hwnd, IDACCEPT);
            }
            else
            {
                ::EndDialog(hwnd, IDREJECT);
            }
        }
        --_this->m_timeoutCount;

        // Flash if necessary
        if (_this->m_foreground_hack)
        {
            if (::GetWindowThreadProcessId(::GetForegroundWindow(), nullptr) != ::GetCurrentProcessId())
            {
                _this->m_flash_state = !_this->m_flash_state;
                ::FlashWindow(hwnd, _this->m_flash_state);
            }
            else
            {
                _this->m_foreground_hack = FALSE;
            }
        }

        // Update the displayed count
        std::array<char, 256U> temp;
        if (_this->m_acceptOnTimeout)
        {
            ::sprintf_s(temp.data(), temp.size(), "AutoAccept: %u", (_this->m_timeoutCount));
        }
        else
        {
            ::sprintf_s(temp.data(), temp.size(), "AutoReject: %u", (_this->m_timeoutCount));
        }
        ::SetDlgItemText(hwnd, IDC_ACCEPT_TIMEOUT, temp.data());
        break;

        // Dialog has just received a command
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
            // User clicked Accept or pressed return
        case IDACCEPT:
        case IDOK:
            ::EndDialog(hwnd, IDACCEPT);
            return TRUE;

        case IDREJECT:
        case IDCANCEL:
            ::EndDialog(hwnd, IDREJECT);
            return TRUE;
        };

        break;

        // Window is being destroyed!  (Should never happen)
    case WM_DESTROY:
        ::DeleteObject(hbmBkGnd);
        ::EndDialog(hwnd, IDREJECT);
        return TRUE;
    }
    return 0;
}