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

extern LONG EnableRemoteInputs;
extern LONG DisableLocalInputs;  //
extern LONG EnableJapInput;      //
extern LONG FileTransferEnabled; //
extern LONG FTUserImpersonation; //
extern LONG BlankMonitorEnabled; //
extern LONG BlankInputsOnly;     //PGM
extern LONG FTTimeout;           //

bool initdone3{ false };
BOOL CALLBACK DlgProcOFT(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        initdone3 = false;
        ::SendMessage(::GetDlgItem(hwnd, IDC_FILETRANSFER), BM_SETCHECK, static_cast<WPARAM>(FileTransferEnabled), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_FTUSERIMPERSONATION_CHECK), BM_SETCHECK, static_cast<WPARAM>(FTUserImpersonation), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_BLANK), BM_SETCHECK, static_cast<WPARAM>(BlankMonitorEnabled), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_BLANK2), BM_SETCHECK, static_cast<WPARAM>(BlankInputsOnly), 0); //PGM
        ::SendMessage(::GetDlgItem(hwnd, IDC_JAP_INPUTS), BM_SETCHECK, static_cast<WPARAM>(EnableJapInput), 0);

        ::SendMessage(::GetDlgItem(hwnd, IDC_DISABLE_LOCAL_INPUTS), BM_SETCHECK, static_cast<WPARAM>(DisableLocalInputs), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_JAP_INPUTS), BM_SETCHECK, static_cast<WPARAM>(EnableJapInput), 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_DISABLE_INPUTS), BM_SETCHECK, !EnableRemoteInputs, 0);

        ::SetDlgItemInt(hwnd, IDC_FTTIMEOUT, static_cast<UINT>(FTTimeout), FALSE);

        ::EnableWindow(::GetDlgItem(hwnd, IDC_FTUSERIMPERSONATION_CHECK), FileTransferEnabled);
        ::EnableWindow(::GetDlgItem(hwnd, IDC_FTTIMEOUT), FileTransferEnabled);

        initdone3 = true;
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_HELP2:
            if (lParam == 2)

            {
                std::array<char, 256U> link;
                std::strcpy(link.data(), "https://uvnc.com/webhelp/");
                std::strcat(link.data(), "input");
                std::strcat(link.data(), ".html");
                ::ShellExecute(::GetDesktopWindow(), "open", link.data(), "", nullptr, SW_SHOWNORMAL);
            }
            break;
        case IDOK:

            EnableRemoteInputs = !::SendDlgItemMessage(hwnd, IDC_DISABLE_INPUTS, BM_GETCHECK, 0, 0);
            DisableLocalInputs = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_DISABLE_LOCAL_INPUTS, BM_GETCHECK, 0, 0));
            EnableJapInput = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_JAP_INPUTS, BM_GETCHECK, 0, 0));
            FileTransferEnabled = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_FILETRANSFER, BM_GETCHECK, 0, 0));
            FTUserImpersonation = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_FTUSERIMPERSONATION_CHECK, BM_GETCHECK, 0, 0));
            BlankMonitorEnabled = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_BLANK, BM_GETCHECK, 0, 0));
            BlankInputsOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_BLANK2, BM_GETCHECK, 0, 0)); //PGM
            BOOL ok1;
            FTTimeout = static_cast<LONG>(::GetDlgItemInt(hwnd, IDC_FTTIMEOUT, &ok1, TRUE));
            break;
        case IDCANCEL:
            ::EndDialog(hwnd, IDCANCEL);
            return TRUE;
        case IDC_FILETRANSFER:
            FileTransferEnabled = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_FILETRANSFER, BM_GETCHECK, 0, 0));
            ::EnableWindow(::GetDlgItem(hwnd, IDC_FTUSERIMPERSONATION_CHECK), FileTransferEnabled);
            ::EnableWindow(::GetDlgItem(hwnd, IDC_FTTIMEOUT), FileTransferEnabled);
            break;
        case IDC_BLANK:
            BlankMonitorEnabled = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_BLANK, BM_GETCHECK, 0, 0));
            ::EnableWindow(::GetDlgItem(hwnd, IDC_BLANK2), BlankMonitorEnabled); //PGM
            if (!BlankMonitorEnabled)
            { //PGM
                ::SendMessage(::GetDlgItem(hwnd, IDC_BLANK2), BM_SETCHECK, static_cast<WPARAM>(BlankMonitorEnabled), 0);
            } //PGM
            BlankInputsOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_BLANK2, BM_GETCHECK, 0, 0)); //PGM
            break;
        case IDC_BLANK2:                                                                     //PGM
            BlankInputsOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_BLANK2, BM_GETCHECK, 0, 0)); //PGM
            break;                                                                           //PGM
        }
        return 0;
    }

    return 0;
}
