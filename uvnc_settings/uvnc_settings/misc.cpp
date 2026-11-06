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

extern LONG DisableTrayIcon;
extern LONG Rdpmode;
extern LONG DefaultScale;
extern LONG Avilog;
extern char path[512U];
extern LONG DebugMode;
extern LONG DebugLevel;
extern LONG BUseRegistry;
extern LONG kickrdp;
extern std::array<char, 256U> servicecmdline;


bool initdone6{ false };
BOOL CALLBACK DlgProcMISC(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        initdone6 = false;
        ::SendMessage(::GetDlgItem(hwnd, IDC_DISABLETRAY), BM_SETCHECK, static_cast<WPARAM>(DisableTrayIcon), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_RDPMODE), BM_SETCHECK, static_cast<WPARAM>(Rdpmode), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_KICKRDP), BM_SETCHECK, static_cast<WPARAM>(kickrdp), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_DISINI), BM_SETCHECK, static_cast<WPARAM>(BUseRegistry), 0);
        ::SetDlgItemInt(hwnd, IDC_SCALE, static_cast<UINT>(DefaultScale), false);
        ::SetDlgItemText(hwnd, IDC_SCL, servicecmdline.data());
        bool debug{ false };
        if (DebugMode >= 2)
        {
            debug = true;
            ::CheckDlgButton(hwnd, IDC_LOG, BST_CHECKED);
        }
        else
        {
            ::CheckDlgButton(hwnd, IDC_LOG, BST_UNCHECKED);
        }

        if (DebugMode == 2)
        {
            ::SendMessage(::GetDlgItem(hwnd, IDC_FILE), BM_SETCHECK, TRUE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_WINDOW), BM_SETCHECK, FALSE, 0);
        }
        if (DebugMode == 4)
        {
            ::SendMessage(::GetDlgItem(hwnd, IDC_WINDOW), BM_SETCHECK, TRUE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_FILE), BM_SETCHECK, FALSE, 0);
        }

        if (DebugLevel >= 10)
        {
            ::SendMessage(::GetDlgItem(hwnd, IDC_ALL), BM_SETCHECK, TRUE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_INFO), BM_SETCHECK, FALSE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_WARN), BM_SETCHECK, FALSE, 0);
        }
        if (DebugLevel == 9)
        {
            ::SendMessage(::GetDlgItem(hwnd, IDC_ALL), BM_SETCHECK, FALSE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_INFO), BM_SETCHECK, TRUE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_WARN), BM_SETCHECK, FALSE, 0);
        }
        if (DebugLevel <= 8)
        {
            ::SendMessage(::GetDlgItem(hwnd, IDC_ALL), BM_SETCHECK, FALSE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_INFO), BM_SETCHECK, FALSE, 0);
            ::SendMessage(::GetDlgItem(hwnd, IDC_WARN), BM_SETCHECK, TRUE, 0);
        }
        ::EnableWindow(::GetDlgItem(hwnd, IDC_WINDOW), debug);
        ::EnableWindow(::GetDlgItem(hwnd, IDC_FILE), debug);
        ::EnableWindow(::GetDlgItem(hwnd, IDC_ALL), debug);
        ::EnableWindow(::GetDlgItem(hwnd, IDC_INFO), debug);
        ::EnableWindow(::GetDlgItem(hwnd, IDC_WARN), debug);



        if (Avilog)
        {
            if (std::strlen(path) == 0)
            {
                std::array<char, MAX_PATH> WORKDIR;
                if (::GetModuleFileName(nullptr, WORKDIR.data(), static_cast<DWORD>(WORKDIR.size())))
                {
                    char* p = std::strrchr(WORKDIR.data(), '\\');
                    if (p == nullptr)
                    {
                        return true;
                    }
                    *p = '\0';
                }
                std::strcpy(path, WORKDIR.data());
            }

            ::SetDlgItemText(hwnd, IDC_EDIT_PATH, path);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_EDIT_PATH), true);
            ::CheckDlgButton(hwnd, IDC_VIDEO, BST_CHECKED);
        }
        else
        {
            if (std::strlen(path) == 0)
            {
                std::array<char, MAX_PATH> WORKDIR;
                if (::GetModuleFileName(nullptr, WORKDIR.data(), static_cast<DWORD>(WORKDIR.size())))
                {
                    char* p = std::strrchr(WORKDIR.data(), '\\');
                    if (p == nullptr)
                    {
                        return true;
                    }
                    *p = '\0';
                }
                std::strcpy(path, WORKDIR.data());
            }
            ::SetDlgItemText(hwnd, IDC_EDIT_PATH, path);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_EDIT_PATH), false);
            ::CheckDlgButton(hwnd, IDC_VIDEO, BST_UNCHECKED);
        }

        initdone6 = true;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_HELP2:
            if (lParam == 5)

            {
                std::array<char, 256U> link;
                std::strcpy(link.data(), "https://uvnc.com/webhelp/");
                std::strcat(link.data(), "misc");
                std::strcat(link.data(), ".html");
                ::ShellExecute(::GetDesktopWindow(), "open", link.data(), "", nullptr, SW_SHOWNORMAL);
            }
            break;
        case IDOK: {
            DisableTrayIcon = static_cast<LONG>(::IsDlgButtonChecked(hwnd, IDC_DISABLETRAY));
            Rdpmode = static_cast<LONG>(::IsDlgButtonChecked(hwnd, IDC_RDPMODE));
            BUseRegistry = static_cast<LONG>(::IsDlgButtonChecked(hwnd, IDC_DISINI));
            kickrdp = static_cast<LONG>(::IsDlgButtonChecked(hwnd, IDC_KICKRDP));
            DefaultScale = static_cast<LONG>(::GetDlgItemInt(hwnd, IDC_SCALE, nullptr, FALSE));
            if ((DefaultScale < 1) || (DefaultScale > 9))
            {
                DefaultScale = 1;
            }
            Avilog = static_cast<LONG>(::IsDlgButtonChecked(hwnd, IDC_VIDEO));
            ::GetDlgItemText(hwnd, IDC_EDIT_PATH, path, 512);
            ::GetDlgItemText(hwnd, IDC_SCL, servicecmdline.data(), 256);
            bool log_checked = ::IsDlgButtonChecked(hwnd, IDC_LOG);
            bool window_checked = ::IsDlgButtonChecked(hwnd, IDC_WINDOW);
            bool file_checked = ::IsDlgButtonChecked(hwnd, IDC_FILE);
            bool debug{ false };
            if (log_checked)
            {
                debug = true;
                if (window_checked)
                {
                    DebugMode = 4;
                }
                if (file_checked)
                {
                    DebugMode = 2;
                }
            }
            else
            {
                DebugMode = 0;
            }
            bool all_checked = ::IsDlgButtonChecked(hwnd, IDC_ALL);
            bool info_checked = ::IsDlgButtonChecked(hwnd, IDC_INFO);
            bool warn_checked = ::IsDlgButtonChecked(hwnd, IDC_WARN);
            if (all_checked)
            {
                DebugLevel = 10;
            }
            if (info_checked)
            {
                DebugLevel = 9;
            }
            if (warn_checked)
            {
                DebugLevel = 8;
            }
        }
        break;
        case IDCANCEL:
            ::EndDialog(hwnd, IDCANCEL);
            return TRUE;
        case IDC_LOG: {
            bool log_checked = ::IsDlgButtonChecked(hwnd, IDC_LOG);
            bool window_checked = ::IsDlgButtonChecked(hwnd, IDC_WINDOW);
            bool file_checked = ::IsDlgButtonChecked(hwnd, IDC_FILE);
            bool debug{ false };
            if (log_checked)
            {
                debug = true;
                if (window_checked)
                {
                    DebugMode = 4;
                }
                if (file_checked)
                {
                    DebugMode = 2;
                }
            }
            else
            {
                DebugMode = 0;
            }
            bool all_checked = ::IsDlgButtonChecked(hwnd, IDC_ALL);
            bool info_checked = ::IsDlgButtonChecked(hwnd, IDC_INFO);
            bool warn_checked = ::IsDlgButtonChecked(hwnd, IDC_WARN);
            if (all_checked)
            {
                DebugLevel = 10;
            }
            if (info_checked)
            {
                DebugLevel = 9;
            }
            if (warn_checked)
            {
                DebugLevel = 8;
            }

            ::EnableWindow(::GetDlgItem(hwnd, IDC_WINDOW), debug);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_FILE), debug);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_ALL), debug);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_INFO), debug);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_WARN), debug);
        }
        break;
        case IDC_VIDEO: {
            if (::IsDlgButtonChecked(hwnd, IDC_VIDEO))
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_EDIT_PATH), true);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_EDIT_PATH), false);
            }
            break;
        }
        case IDC_CLEAR: {
            std::array<char, MAX_PATH> WORKDIR;
            std::strcpy(WORKDIR.data(), path);
            std::strcat(WORKDIR.data(), "\\");
            std::strcat(WORKDIR.data(), "codec.cfg");
            ::DeleteFile(WORKDIR.data());
            break;
        }
        }
        return 0;
    }

    return 0;
}
