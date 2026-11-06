// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// AuthDialog.cpp: implementation of the AuthDialog class.

#include "stdhdrs.h"

#include "AuthDialog.h"

#include "common/win32_helpers.h"
#include "Exception.h"
#include "vncviewer.h"

#include <windowsx.h>

#include <array>
#include <cstring>
#include <cwchar>
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

AuthDialog::AuthDialog()
{
    m_passwd[0U] = __T('\0');
    //adzm 2010-05-12 - passphrase
    m_bPassphraseMode = false;
    m_savePassword = false;
    m_hwndStatus = nullptr;
    m_className[0U] = L'\0';
    m_statusWasVisible = false;
}

AuthDialog::~AuthDialog() {}

void AuthDialog::SetStatusWindow(HWND hwndStatus, const wchar_t* const className)
{
    m_hwndStatus = hwndStatus;
    if (className)
    {
        ::wcscpy_s(m_className.data(), m_className.size(), className);
    }
    else
    {
        m_className[0U] = L'\0';
    }
}

int AuthDialog::DoDialog(const DialogType dialogTypeIn, TCHAR IN_host[MAX_HOST_NAME_LEN], const int IN_port, char hexIn[24U],
                         char catchphraseIn[1024U])
{
    std::array<TCHAR, 10U> tempchar;
    ::_tcscpy_s(_host.data(), _host.size(), IN_host);
    ::_tcscat_s(_host.data(), _host.size(), _T(":"));
    _itot_s(IN_port, tempchar.data(), tempchar.size(), 10);
    ::_tcscat_s(_host.data(), _host.size(), tempchar.data());
    this->dialogType = dialogTypeIn;
    std::strcpy(this->hex, hexIn);
    std::strcpy(this->catchphrase.data(), catchphraseIn);

    // Hide status window during authentication if classname is used
    m_statusWasVisible = false;
    if ((std::wcslen(m_className.data()) > 0) && m_hwndStatus && IsWindowVisible(m_hwndStatus))
    {
        m_statusWasVisible = true;
        ::ShowWindow(m_hwndStatus, SW_HIDE);
    }

    extern HINSTANCE m_hInstResDLL;
    int result{ 0 };
    switch (dialogTypeIn)
    {
    case dtUserPass:
        result = static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG), nullptr, (DLGPROC)DlgProc, (LONG_PTR)this));
        break;
    case dtPass:
        result =
            static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG3), nullptr, (DLGPROC)DlgProc1, (LONG_PTR)this));
        break;
    case dtUserPassNotEncryption:
        result = static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG2), nullptr, (DLGPROC)DlgProc, (LONG_PTR)this));
        break;
    case dtPassUpgrade:
        result =
            static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG1), nullptr, (DLGPROC)DlgProc1, (LONG_PTR)this));
        break;
    case dtUserPassRSA:
        m_bPassphraseMode = true;
        result = static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG4), nullptr, (DLGPROC)DlgProc, (LONG_PTR)this));
        break;
    case dtPassRSA:
        m_bPassphraseMode = true;
        result =
            static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG5), nullptr, (DLGPROC)DlgProc1, (LONG_PTR)this));
        break;
    case dtUserToken:
        result = static_cast<int>(::DialogBoxParam(m_hInstResDLL, DIALOG_MAKEINTRESOURCE(IDD_AUTH_DIALOG), nullptr, (DLGPROC)DlgProc, (LONG_PTR)this));
        break;
    }

    // Restore status window after authentication if classname is used
    if (m_statusWasVisible && m_hwndStatus)
    {
        ::ShowWindow(m_hwndStatus, SW_SHOW);
    }

    return result;
}

BOOL CALLBACK AuthDialog::DlgProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    // This is a static method, so we don't know which instantiation we're
    // dealing with. But we can get a pseudo-this from the parameter to
    // WM_INITDIALOG, which we therafter store with the window and retrieve
    // as follows:

    AuthDialog* _this = helper::SafeGetWindowUserData<AuthDialog>(hwnd);

    switch (uMsg)
    {

    case WM_INITDIALOG: {
        helper::SafeSetWindowUserData(hwnd, lParam);

        _this = (AuthDialog*)lParam;

        if (_this->dialogType == dtUserPassRSA)
        {
            Edit_LimitText(::GetDlgItem(hwnd, IDC_PASSWD_EDIT), 256);
        }
        else if (_this->dialogType == dtUserToken)
        {
            Edit_LimitText(::GetDlgItem(hwnd, IDC_PASSWD_EDIT), 64);
        }
        else
        {
            Edit_LimitText(::GetDlgItem(hwnd, IDC_PASSWD_EDIT), 32);
        }
        //CentreWindow(hwnd);
        std::array<TCHAR, MAX_HOST_NAME_LEN> tempchar;
        ::GetWindowText(hwnd, tempchar.data(), MAX_HOST_NAME_LEN);
        ::_tcscat_s(tempchar.data(), tempchar.size(), _T("   "));
        ::_tcscat_s(tempchar.data(), tempchar.size(), _this->_host.data());
        ::SetWindowText(hwnd, tempchar.data());
        ::SetForegroundWindow(hwnd);
        {
            std::array<wchar_t, 1024U> _wcp;
            ::MultiByteToWideChar(CP_UTF8, 0, _this->catchphrase.data(), -1, _wcp.data(), 1024);
            ::SetDlgItemTextW(hwnd, IDC_CATCHPHRASE, _wcp.data());
        }
        {
            std::array<wchar_t, 64U> _whx;
            ::MultiByteToWideChar(CP_UTF8, 0, _this->hex, -1, _whx.data(), 64);
            ::SetDlgItemTextW(hwnd, IDC_SIGNATURE, _whx.data());
        }
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK: {
            UINT res = ::GetDlgItemTextA(hwnd, IDC_PASSWD_EDIT, _this->m_passwd.data(), 256);
            res = ::GetDlgItemTextA(hwnd, IDD_DOMAIN, _this->m_domain.data(), 256);
            res = ::GetDlgItemTextA(hwnd, IDD_USER_NAME, _this->m_user.data(), 256);
            _this->m_savePassword = (::IsDlgButtonChecked(hwnd, IDC_SAVE_PASSWORD) == BST_CHECKED);

            ::EndDialog(hwnd, TRUE);

            return TRUE;
        }
        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return 0;
}

BOOL CALLBACK AuthDialog::DlgProc1(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    // This is a static method, so we don't know which instantiation we're
    // dealing with. But we can get a pseudo-this from the parameter to
    // WM_INITDIALOG, which we therafter store with the window and retrieve
    // as follows:
    AuthDialog* _this = helper::SafeGetWindowUserData<AuthDialog>(hwnd);

    switch (uMsg)
    {

    case WM_INITDIALOG: {
        _this = (AuthDialog*)lParam;
        helper::SafeSetWindowUserData(hwnd, lParam);
        //adzm 2010-05-12 - passphrase
        Edit_LimitText(::GetDlgItem(hwnd, IDC_PASSWD_EDIT), _this->m_bPassphraseMode ? 128 : 8);
        //CentreWindow(hwnd);
        std::array<TCHAR, MAX_HOST_NAME_LEN> tempchar;
        ::GetWindowText(hwnd, tempchar.data(), MAX_HOST_NAME_LEN);
        ::_tcscat_s(tempchar.data(), tempchar.size(), _T("   "));
        ::_tcscat_s(tempchar.data(), tempchar.size(), _this->_host.data());
        ::SetWindowText(hwnd, tempchar.data());
        {
            std::array<wchar_t, 1024U> _wcp;
            ::MultiByteToWideChar(CP_UTF8, 0, _this->catchphrase.data(), -1, _wcp.data(), 1024);
            ::SetDlgItemTextW(hwnd, IDC_CATCHPHRASE, _wcp.data());
        }
        {
            std::array<wchar_t, 64U> _whx;
            ::MultiByteToWideChar(CP_UTF8, 0, _this->hex, -1, _whx.data(), 64);
            ::SetDlgItemTextW(hwnd, IDC_SIGNATURE, _whx.data());
        }
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_TRAY));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK: {
            [[maybe_unused]] UINT res = ::GetDlgItemTextA(hwnd, IDC_PASSWD_EDIT, _this->m_passwd.data(), 256);
            _this->m_savePassword = (::IsDlgButtonChecked(hwnd, IDC_SAVE_PASSWORD) == BST_CHECKED);
            ::EndDialog(hwnd, TRUE);

            return TRUE;
        }
        case IDCANCEL:
            ::EndDialog(hwnd, FALSE);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        return TRUE;
    }
    return 0;
}
