// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "DlgChangePassword.h"

#include "common/win32_helpers.h"
#include "resource.h"
#include "SettingsManager.h"

#include <array>
#include <cstring>

extern HINSTANCE hInstResDLL;

INT_PTR CALLBACK dialogProcChangePassword(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    DlgChangePassword* _this;
    if (uMsg == WM_INITDIALOG)
    {
        _this = (DlgChangePassword*)lParam;
        helper::SafeSetWindowUserData(hwnd, lParam);
    }
    else
    {
        _this = (DlgChangePassword*)helper::SafeGetWindowUserData<DlgChangePassword>(hwnd);
    }

    switch (uMsg)
    {
    case WM_INITDIALOG:
        return _this->InitDialog(hwnd);
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        case IDOK:
            _this->onOK(hwnd);
            return TRUE;
        default:
            return TRUE;
        }
    case WM_CLOSE:
        ::DestroyWindow(hwnd);
        return TRUE;
    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return 0;
}
DlgChangePassword::DlgChangePassword() {}
DlgChangePassword::~DlgChangePassword() {}

int DlgChangePassword::ShowDlg(const HWND parentWindow, const char* const title, const int passwordSize)
{
    ::strcpy_s(this->title, title);
    this->passwordSize = passwordSize;

    const int a =
        ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_CHANGE_PASSWORD), parentWindow, dialogProcChangePassword, (LPARAM)this);
    return a;
}

bool DlgChangePassword::InitDialog(const HWND hwnd)
{
    DlgChangePasswordHwnd = hwnd;
    ::SetForegroundWindow(hwnd);
    HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
    ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    ::SetWindowText(hwnd, title);
    ::SendMessage(::GetDlgItem(DlgChangePasswordHwnd, IDC_PASSWORD_1), EM_SETLIMITTEXT, passwordSize, 0);
    ::SendMessage(::GetDlgItem(DlgChangePasswordHwnd, IDC_PASSWORD_2), EM_SETLIMITTEXT, passwordSize, 0);
    ::SetDlgItemText(hwnd, IDC_PASSWORD_1, "~~~~~~~~");
    ::SetDlgItemText(hwnd, IDC_PASSWORD_2, "~~~~~~~~");
    return true;
}

void DlgChangePassword::onOK(const HWND hwnd)
{
    std::array<char, 1024U> passwd2;
    std::memset(passwd, '\0', 1024);
    std::memset(passwd2.data(), '\0', 1024);
    ::GetDlgItemText(hwnd, IDC_PASSWORD_1, (LPSTR)&passwd, 1024);
    ::GetDlgItemText(hwnd, IDC_PASSWORD_2, (LPSTR)passwd2.data(), 1024);
    if (((std::strlen(passwd) == 0) || (std::strcmp(passwd, "~~~~~~~~") == 0)) && (settings->getAuthRequired() != false))
    {
        helper::yesUVNCMessageBox(hInstResDLL, hwnd, "Password can not be empty.", "UltraVNC warning", MB_ICONWARNING);
        ::SetFocus(::GetDlgItem(hwnd, IDC_PASSWORD_1));
        return;
    }
    if (((std::strcmp(passwd, passwd2.data()) != 0) || (std::strcmp(passwd, "~~~~~~~~") == 0)))
    {
        helper::yesUVNCMessageBox(hInstResDLL, hwnd, "The verification passwords is not the same.", "UltraVNC warning",
                                  MB_ICONWARNING);
        ::SetFocus(::GetDlgItem(hwnd, IDC_PASSWORD_2));
        return;
    }
    if (std::strcmp(passwd, "~~~~~~~~") == 0)
    {
        ::strcpy_s(passwd, "");
    }
    ::EndDialog(hwnd, TRUE);
}

char* DlgChangePassword::getPassword() { return passwd; }
