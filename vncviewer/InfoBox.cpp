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

#include "../common/Hyperlinks.h"
#include "UltraVNCHelperFunctions.h"
#include "vncviewer.h"

#include <array>
#include <cstddef>
#include <cstring>

char* infomsg2;
const wchar_t* infomsg2_w;
int g_error_nr2;
bool use_wide_msg2{ false };
bool g_disable_sponsor{ false };
extern char buildtime[];
void convertToISO8601(const char* const input, char* const output, const std::size_t size);

// Process the About dialog.
static LRESULT CALLBACK MessageDlgProc2(const HWND hwnd, const UINT iMsg, const WPARAM wParam, [[maybe_unused]] const LPARAM lParam)
{
    switch (iMsg)
    {
    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        //CentreWindow(hwnd);
        ::SetForegroundWindow(hwnd);
        extern char buildtime[];
        std::array<char, 20U> isoTime; // Buffer for ISO output
        convertToISO8601(buildtime, isoTime.data(), sizeof(isoTime));
        std::array<wchar_t, 20U> wisoTime;
        ::MultiByteToWideChar(CP_ACP, 0, isoTime.data(), -1, wisoTime.data(), 20);
        ::SetDlgItemTextW(hwnd, IDC_BUILDTIME, wisoTime.data());

        if (use_wide_msg2)
        {
            ::SetDlgItemTextW(hwnd, IDC_Message2, infomsg2_w);
            if (::_wcsicmp(infomsg2_w, L"Your connection has been rejected.") == 0)
            {
                g_error_nr2 = 1000;
            }
            if (::_wcsicmp(infomsg2_w, L"Local loop-back connections are disabled.") == 0)
            {
                g_error_nr2 = 1001;
            }
        }
        else
        {
            std::array<wchar_t, 1024U> winfomsg2;
            ::MultiByteToWideChar(CP_ACP, 0, infomsg2, -1, winfomsg2.data(), 1024);
            ::SetDlgItemTextW(hwnd, IDC_Message2, winfomsg2.data());
            if ((std::strcmp(infomsg2, "Your connection has been rejected.") == 0))
            {
                g_error_nr2 = 1000;
            }
            if ((std::strcmp(infomsg2, "Local loop-back connections are disabled.") == 0))
            {
                g_error_nr2 = 1001;
            }
        }
        ConvertStaticToHyperlink(hwnd, IDC_FORUMHYPERLINK);
        ConvertStaticToHyperlink(hwnd, IDC_WEBSITE);
        ConvertStaticToHyperlink(hwnd, IDC_GIT);
        ConvertStaticToHyperlink(hwnd, IDC_WEBDOWNLOAD);
        std::array<char, 50U> version{};
        std::array<char, 256U> title{};
        ::strcpy_s(title.data(), title.size(), "UltraVNC Viewer - ");
        ::strcat_s(title.data(), title.size(), GetVersionFromResource(version.data()));
        std::array<wchar_t, 256U> wtitle;
        ::MultiByteToWideChar(CP_ACP, 0, title.data(), -1, wtitle.data(), 256);
        ::SetDlgItemTextW(hwnd, IDC_UVVERSION2, wtitle.data());
        return TRUE;
    }
    case WM_CLOSE:
        ::EndDialog(hwnd, TRUE);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            ::EndDialog(hwnd, TRUE);
        }
        if (LOWORD(wParam) == IDC_FORUMHYPERLINK)
        {
            ::ShellExecuteW(::GetDesktopWindow(), L"open", L"https://forum.uvnc.com/", L"", nullptr, SW_SHOWNORMAL);
        }
        if (LOWORD(wParam) == IDC_WEBSITE)
        {
            ::ShellExecuteW(::GetDesktopWindow(), L"open", L"https://uvnc.com/", L"", nullptr, SW_SHOWNORMAL);
        }
        if (LOWORD(wParam) == IDC_GIT)
        {
            ::ShellExecuteW(::GetDesktopWindow(), L"open", L"https://github.com/ultravnc/UltraVNC", L"", nullptr, SW_SHOWNORMAL);
        }
        if (LOWORD(wParam) == IDC_WEBDOWNLOAD)
        {
            ::ShellExecuteW(::GetDesktopWindow(), L"open", L"https://uvnc.com/downloads/ultravnc.html", L"", nullptr, SW_SHOWNORMAL);
        }
    }
    return FALSE;
}

void ShowMessageBox2(char* const info, const int error_nr)
{
    use_wide_msg2 = false;
    infomsg2 = info;
    g_error_nr2 = error_nr;
    [[maybe_unused]] const int res = static_cast<int>(::DialogBox(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_APP_MESSAGE2), nullptr, reinterpret_cast<DLGPROC>(MessageDlgProc2)));
}

void ShowMessageBox2(const wchar_t* const info, const int error_nr)
{
    use_wide_msg2 = true;
    infomsg2_w = info;
    g_error_nr2 = error_nr;
    [[maybe_unused]] const int res = static_cast<int>(::DialogBox(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_APP_MESSAGE2), nullptr, reinterpret_cast<DLGPROC>(MessageDlgProc2)));
}
