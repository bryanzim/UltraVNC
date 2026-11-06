// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "stdafx.h"

#include "resource.h"

#include <array>
#include <cstring>

extern LONG Primary;
extern LONG Secondary;
extern LONG TurboMode;
extern LONG PollUnderCursor;
extern LONG PollForeground;
extern LONG PollFullScreen;
extern LONG PollConsoleOnly;
extern LONG PollOnEventOnly;
extern LONG Driver;
extern bool ddEngine;
extern LONG Hook;
extern LONG RemoveWallpaper;
extern LONG RemoveAero;
bool CheckVideoDriver(bool Box);
extern LONG MaxCpu;
extern LONG MaxFps;

bool initdone5{ false };
BOOL CALLBACK DlgProcCAP(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        initdone5 = false;
        ::SendMessage(::GetDlgItem(hwnd, IDC_TURBOMODE), BM_SETCHECK, static_cast<WPARAM>(TurboMode), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_DRIVER), BM_SETCHECK, static_cast<WPARAM>(Driver), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_HOOK), BM_SETCHECK, static_cast<WPARAM>(Hook), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_POLL_FULLSCREEN), BM_SETCHECK, static_cast<WPARAM>(PollFullScreen), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_POLL_FOREGROUND), BM_SETCHECK, static_cast<WPARAM>(PollForeground), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_POLL_UNDER_CURSOR), BM_SETCHECK, static_cast<WPARAM>(PollUnderCursor), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_CONSOLE_ONLY), BM_SETCHECK, static_cast<WPARAM>(PollConsoleOnly), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_REMOVE_WALLPAPER), BM_SETCHECK, static_cast<WPARAM>(RemoveWallpaper), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_REMOVE_Aero), BM_SETCHECK, static_cast<WPARAM>(RemoveAero), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_ONEVENT_ONLY), BM_SETCHECK, static_cast<WPARAM>(PollOnEventOnly), 0);

        if (ddEngine)
        {
            ::ShowWindow(::GetDlgItem(hwnd, IDC_CHECKDRIVER), false);
            //ShowWindow(GetDlgItem(hwnd, IDC_STATICELEVATED), false);
            ::SetWindowText(::GetDlgItem(hwnd, IDC_DRIVER), "Desktop Duplication (restart on change required)");
            RECT rect;
            ::GetWindowRect(::GetDlgItem(hwnd, IDC_DRIVER), &rect);
            POINT pt;
            pt.x = rect.left;
            pt.y = rect.top;
            ::ScreenToClient(hwnd, &pt);
            ::MoveWindow(::GetDlgItem(hwnd, IDC_DRIVER), pt.x, pt.y, 410, 20, FALSE);
        }

        ::EnableWindow(::GetDlgItem(hwnd, IDC_CONSOLE_ONLY), (PollUnderCursor || PollForeground));
        ::EnableWindow(::GetDlgItem(hwnd, IDC_ONEVENT_ONLY), (PollUnderCursor || PollForeground));

        ::SendMessage(::GetDlgItem(hwnd, IDC_PRIM), BM_SETCHECK, static_cast<WPARAM>(Primary), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_SEC), BM_SETCHECK, static_cast<WPARAM>(Secondary), 0);
        ::SetDlgItemInt(hwnd, IDC_MAXCPU, static_cast<UINT>(MaxCpu), false);
        ::SetDlgItemInt(hwnd, IDC_MAXFPS, static_cast<UINT>(MaxFps), false);
        initdone5 = true;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_HELP2:
            if (lParam == 4)
            {
                std::array<char, 256U> link;
                std::strcpy(link.data(), "https://uvnc.com/webhelp/");
                std::strcat(link.data(), "screencapture");
                std::strcat(link.data(), ".html");
                ::ShellExecute(::GetDesktopWindow(), "open", link.data(), "", nullptr, SW_SHOWNORMAL);
            }
            break;
        case IDOK: {
            TurboMode = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_TURBOMODE, BM_GETCHECK, 0, 0));
            PollUnderCursor = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_POLL_UNDER_CURSOR, BM_GETCHECK, 0, 0));
            PollForeground = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_POLL_FOREGROUND, BM_GETCHECK, 0, 0));
            PollFullScreen = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_POLL_FULLSCREEN, BM_GETCHECK, 0, 0));
            PollConsoleOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_CONSOLE_ONLY, BM_GETCHECK, 0, 0));
            PollOnEventOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_ONEVENT_ONLY, BM_GETCHECK, 0, 0));
            Driver = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_DRIVER, BM_GETCHECK, 0, 0));
            Primary = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_PRIM, BM_GETCHECK, 0, 0));
            Secondary = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_SEC, BM_GETCHECK, 0, 0));
            Hook = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_HOOK, BM_GETCHECK, 0, 0));
            RemoveWallpaper = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_REMOVE_WALLPAPER, BM_GETCHECK, 0, 0));
            RemoveAero = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_REMOVE_Aero, BM_GETCHECK, 0, 0));
            MaxCpu = static_cast<LONG>(::GetDlgItemInt(hwnd, IDC_MAXCPU, nullptr, FALSE));
            MaxFps = static_cast<LONG>(::GetDlgItemInt(hwnd, IDC_MAXFPS, nullptr, FALSE));
        }
        break;
        case IDCANCEL:
            ::EndDialog(hwnd, IDCANCEL);
            return TRUE;
        case IDC_POLL_FOREGROUND:
        case IDC_POLL_UNDER_CURSOR:
            // User has clicked on one of the polling mode buttons
            // affected by the pollconsole and pollonevent options
            {
                // Get the poll-mode buttons
                HWND hPollForeground = ::GetDlgItem(hwnd, IDC_POLL_FOREGROUND);
                HWND hPollUnderCursor = ::GetDlgItem(hwnd, IDC_POLL_UNDER_CURSOR);

                // Determine whether to enable the modifier options
                BOOL enabled = (::SendMessage(hPollForeground, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
                               (::SendMessage(hPollUnderCursor, BM_GETCHECK, 0, 0) == BST_CHECKED);

                HWND hPollConsoleOnly = ::GetDlgItem(hwnd, IDC_CONSOLE_ONLY);
                ::EnableWindow(hPollConsoleOnly, enabled);

                HWND hPollOnEventOnly = ::GetDlgItem(hwnd, IDC_ONEVENT_ONLY);
                ::EnableWindow(hPollOnEventOnly, enabled);
            }
            return TRUE;
        case IDC_CHECKDRIVER: {
            CheckVideoDriver(1);
        }
            return TRUE;
        }
        return 0;
    }

    return 0;
}
