#include <array>
// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#pragma once

#ifndef MAX_HOST_NAME_LEN
#define MAX_HOST_NAME_LEN 250
#endif

enum DialogType
{
    dtUserPass,
    dtPass,
    dtUserPassNotEncryption,
    dtPassUpgrade,
    dtUserPassRSA,
    dtPassRSA,
    dtUserToken
};

class AuthDialog
{
public:
    explicit AuthDialog();
    virtual ~AuthDialog();
    int DoDialog(DialogType dialogType, TCHAR IN_host[MAX_HOST_NAME_LEN], int IN_port, char hex[24U] = "",
                 char catchphrase[1024U] = "");
    void SetStatusWindow(HWND hwndStatus, const wchar_t* className);
    std::array<char, 256U> m_passwd;
    std::array<char, 256U> m_domain;
    std::array<char, 256U> m_user;
    static BOOL CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK DlgProc1(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


    //adzm 2010-05-12 - passphrase
    bool m_bPassphraseMode;
    bool m_savePassword;
    std::array<wchar_t, MAX_HOST_NAME_LEN> _host;
    int _port;
    DialogType dialogType;
    char hex[24U]{};
    std::array<char, 1024U> catchphrase{};

    // For hiding status window during authentication when classname is used
    HWND m_hwndStatus;
    std::array<wchar_t, 255U> m_className;
    bool m_statusWasVisible;
};
