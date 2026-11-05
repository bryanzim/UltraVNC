// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// vncListDlg.cpp

// Implementation of the vncListDlg dialog!

#include "stdhdrs.h"

#include "vncListDlg.h"

#include "common/win32_helpers.h"
#include "SettingsManager.h"
#include "winvnc.h"

#include <array>
// [v1.0.2-jp1 fix] Load resouce from dll

extern HINSTANCE hInstResDLL;
HWND listDlgHwnd = nullptr;

//
//
//
vncListDlg::vncListDlg() { m_dlgvisible = FALSE; }

//
//
//
vncListDlg::~vncListDlg() {}

//
//
//
BOOL vncListDlg::Init(vncServer* pServer)
{
    m_pServer = pServer;
    return TRUE;
}

//
//
//
void vncListDlg::Display()
{
    if (!m_dlgvisible)
    {
        ::DialogBoxParam(hInstResDLL, MAKEINTRESOURCE(IDD_LIST_DLG), nullptr, (DLGPROC)DialogProc, (LONG_PTR)this);
    }
}

//
//
//
BOOL CALLBACK vncListDlg::DialogProc(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    vncListDlg* _this = helper::SafeGetWindowUserData<vncListDlg>(hwnd);
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        HICON hIcon = ::LoadIcon(::GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_WINVNC));
        ::SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        ::SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        helper::SafeSetWindowUserData(hwnd, lParam);
        _this = (vncListDlg*)lParam;
        listDlgHwnd = hwnd;
        //vncClientList::iterator i;
        HWND hList = ::GetDlgItem(hwnd, IDC_VIEWERS_LISTBOX);

        _this->m_pServer->ListAuthClients(hList);
        ::SendMessage(hList, LB_SETCURSEL, -1, 0);

        // adzm 2009-07-05
        HWND hPendingList = ::GetDlgItem(hwnd, IDC_PENDING_LISTBOX);
        _this->m_pServer->ListUnauthClients(hPendingList);

        ::SetForegroundWindow(hwnd);
        _this->m_dlgvisible = TRUE;
        if (!settings->getAllowEditClients())
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_KILL_B), false);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_KILL_B), true);
        }

        // Allow Text Chat if one client only
        /*
        EnableWindow(GetDlgItem(hwnd, IDC_TEXTCHAT_B),
                     _this->m_pServer->AuthClientCount() == 1 ? TRUE : FALSE);
        */
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {

        case IDCANCEL:
        case IDOK:
            ::EndDialog(hwnd, TRUE);
            _this->m_dlgvisible = FALSE;
            return TRUE;

        case IDC_KILL_B: {
            HWND hList = ::GetDlgItem(hwnd, IDC_VIEWERS_LISTBOX);
            LRESULT nSelected = ::SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (nSelected != LB_ERR)
            {
                std::array<char, 128U> szClient;
                if (::SendMessage(hList, LB_GETTEXT, nSelected, (LPARAM)szClient.data()) > 0)
                {
                    _this->m_pServer->KillClient(szClient.data());
                }
            }
            //EndDialog(hwnd, TRUE);
            //_this->m_dlgvisible = FALSE;
            return TRUE;
        }
        break;

        case IDC_TEXTCHAT_B: {
            HWND hList = ::GetDlgItem(hwnd, IDC_VIEWERS_LISTBOX);
            LRESULT nSelected = ::SendMessage(hList, LB_GETCURSEL, 0, 0);
            if (nSelected != LB_ERR)
            {
                std::array<char, 128U> szClient;
                if (::SendMessage(hList, LB_GETTEXT, nSelected, (LPARAM)szClient.data()) > 0)
                {
                    _this->m_pServer->TextChatClient(szClient.data());
                }
            }
            //EndDialog(hwnd, TRUE);
            //_this->m_dlgvisible = FALSE;
            return TRUE;
        }
        break;
        }
        break;

    case WM_UPDATEVIEWERS: {
        HWND hList = ::GetDlgItem(hwnd, IDC_VIEWERS_LISTBOX);
        _this->m_pServer->ListAuthClients(hList);
        ::SendMessage(hList, LB_SETCURSEL, -1, 0);
        HWND hPendingList = ::GetDlgItem(hwnd, IDC_PENDING_LISTBOX);
        _this->m_pServer->ListUnauthClients(hPendingList);

        ::SetForegroundWindow(hwnd);
        _this->m_dlgvisible = TRUE;
        if (!settings->getAllowEditClients())
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_KILL_B), false);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_KILL_B), true);
        }
        return TRUE;
    }

    case WM_DESTROY:
        ::EndDialog(hwnd, FALSE);
        _this->m_dlgvisible = FALSE;
        return TRUE;
    }
    return 0;
}
