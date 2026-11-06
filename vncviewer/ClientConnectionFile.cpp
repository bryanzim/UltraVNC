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


#include "stdhdrs.h"

#include "ClientConnection.h"
#include "common/win32_helpers.h"
#include "Exception.h"
//AaronP
#include "SessionDialog.h"
#include "UltraVNCHelperFunctions.h"
//EndAaronP
#include "vncauth.h"
#include "vncviewer.h"

#include <array>
#include <cstring>

using namespace helper;
extern HINSTANCE m_hInstResDLL;

extern std::array<wchar_t, 64U> sz_K1;
extern std::array<wchar_t, 64U> sz_K2;
extern std::array<wchar_t, 128U> sz_K3;
extern std::array<wchar_t, 64U> sz_K4;
extern std::array<wchar_t, 64U> sz_K5;
extern std::array<wchar_t, 64U> sz_K6;
extern std::array<wchar_t, 64U> sz_K7;
extern bool config_specified;

// This file contains the code for saving and loading connection info.

static OPENFILENAME ofn;

void ofnInit()
{
    static TCHAR filter[] = _T("VNC files (*.vnc)\0*.vnc\0")
                            _T("All files (*.*)\0*.*\0");
    std::memset((void*)&ofn, 0, sizeof(OPENFILENAME));

    // sf@2002 v1.1.1 - OPENFILENAME is Plateforme dependent !
    // Under Windows NT4, the dialog box wouldn't appear if we don't use OPENFILENAME_SIZE_VERSION_400
    // when compiling using BCC55 under Windows 2000.
#ifdef OPENFILENAME_SIZE_VERSION_400
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
#else
    ofn.lStructSize = sizeof(OPENFILENAME);
#endif

    ofn.lpstrFilter = filter;
    ofn.nMaxFile = _MAX_PATH;
    ofn.nMaxFileTitle = _MAX_FNAME + _MAX_EXT;
    ofn.lpstrDefExt = _T("vnc");
}

//
// SaveConnection
// Save info about current connection to a file
//

void ClientConnection::SaveConnection()
{
    vnclog.Print(2, _T("Saving connection info\n"));
    std::array<TCHAR, _MAX_PATH> fname;
    std::array<TCHAR, _MAX_FNAME + _MAX_EXT> tname;
    ofnInit();
    int disp = PORT_TO_DISPLAY(m_port);
    ::_sntprintf_s(fname.data(), fname.size(), _TRUNCATE, _T("%.15s-%d.vnc"), m_host.data(), ((disp > 0) && (disp < 100)) ? disp : m_port);
    ofn.hwndOwner = m_hwndcn;
    ofn.lpstrFile = fname.data();
    ofn.lpstrFileTitle = tname.data();
    ofn.Flags = OFN_HIDEREADONLY;
    if (!::GetSaveFileName(&ofn))
    {
        DWORD err = ::CommDlgExtendedError();
        switch (err)
        {
        case 0: // user cancelled
            break;
        case FNERR_INVALIDFILENAME:
            yesUVNCMessageBox(m_hInstResDLL, m_hwndcn, sz_K1.data(), sz_K2.data(), MB_ICONERROR);
            break;
        default:
            vnclog.Print(0, _T("Error %d from GetSaveFileName\n"), err);
            break;
        }
        return;
    }
    vnclog.Print(1, _T("Saving to %s\n"), fname.data());

    ::WritePrivateProfileString(_T("connection"), _T("host"), m_host.data(), fname.data());
    std::array<char, 32U> buf;
    std::array<TCHAR, 32U> bufW;
    ::_stprintf_s(bufW.data(), bufW.size(), _T("%d"), m_port);
    ::WritePrivateProfileString(_T("connection"), _T("port"), bufW.data(), fname.data());

    ::WritePrivateProfileString(_T("connection"), _T("proxyhost"), m_proxyhost.data(), fname.data());
    ::_stprintf_s(bufW.data(), bufW.size(), _T("%d"), m_proxyport);
    ::WritePrivateProfileString(_T("connection"), _T("proxyport"), bufW.data(), fname.data());
    BOOL bCheckboxChecked;
    int yes = yesnoUVNCMessageBox(m_hInstResDLL, m_hwndcn, sz_K3.data(), sz_K4.data(), str50287.data(), str50288.data(), _T(""), bCheckboxChecked);
    if (yes)
    {
        for (int i{ 0 }; i < MAXPWLEN; ++i)
        {
            ::sprintf_s(buf.data() + i * 2, 32 - i * 2, "%02x", static_cast<unsigned int>(m_encPasswd[i]));
        }
    }
    else
    {
        buf[0U] = '\0';
    }
    // Password stored as hex string, use Unicode API for correct filename handling
    std::array<wchar_t, 32U> wbuf;
    ::MultiByteToWideChar(CP_UTF8, 0, buf.data(), -1, wbuf.data(), 32);
    ::WritePrivateProfileStringW(L"connection", L"password", wbuf.data(), fname.data());
    m_opts->SaveOptions(fname.data());
    //m_opts->Register();
}

void ClientConnection::SaveAllowUntrustedServers()
{
    std::array<TCHAR, 10U> buf;
    ::_stprintf_s(buf.data(), buf.size(), _T("%d"), 1);
    if (m_opts->m_UseOnlyDefaultConfigFile)
    {
        ::WritePrivateProfileString(_T("options"), _T("AllowUntrustedServers"), buf.data(), m_opts->getDefaultOptionsFileName());
    }
    else
    {
        std::array<TCHAR, _MAX_PATH> fname;
        std::array<TCHAR, _MAX_FNAME + _MAX_EXT> tname;
        ofnInit();
        int disp = PORT_TO_DISPLAY(m_port);
        ::_sntprintf_s(fname.data(), fname.size(), _TRUNCATE, _T("%.15s-%d.vnc"), m_host.data(),
                     ((disp > 0) && (disp < 100)) ? disp : m_port);
        ofn.hwndOwner = m_hwndcn;
        ofn.lpstrFile = fname.data();
        ofn.lpstrFileTitle = tname.data();
        ofn.Flags = OFN_HIDEREADONLY;
        if (!::GetSaveFileName(&ofn))
        {
            DWORD err = ::CommDlgExtendedError();
            switch (err)
            {
            case 0: // user cancelled
                break;
            case FNERR_INVALIDFILENAME:
                yesUVNCMessageBox(m_hInstResDLL, m_hwndcn, sz_K1.data(), sz_K2.data(), MB_ICONERROR);
                break;
            default:
                vnclog.Print(0, _T("Error %d from GetSaveFileName\n"), err);
                break;
            }
            return;
        }
        ::WritePrivateProfileString(_T("options"), _T("AllowUntrustedServers"), buf.data(), fname.data());
    }
}


void ClientConnection::Save_Latest_Connection()
{
    if (config_specified)
    {
        return;
    }
    vnclog.Print(2, _T("Saving connection info\n"));
    m_opts->SaveOptions(m_opts->getDefaultOptionsFileName());
}

// returns zero if successful
int ClientConnection::LoadConnection(const wchar_t* fname, const bool fFromDialog, const bool defaultOption)
{
    // The Connection Profile ".vnc" has been required from Connection Session Dialog Box
    if (fFromDialog && !defaultOption)
    {
        std::array<TCHAR, _MAX_PATH> tfname;
        std::array<TCHAR, _MAX_FNAME + _MAX_EXT> tname;
        ::_tcscpy_s(tfname.data(), tfname.size(), fname);
        ofnInit();
        ofn.hwndOwner = m_hSessionDialog;
        ofn.lpstrFile = tfname.data();
        ofn.lpstrFileTitle = tname.data();
        ofn.Flags = OFN_HIDEREADONLY;
        if (::GetOpenFileName(&ofn) == 0)
        {
            return -1;
        }
        // Copy selected filename back to caller's buffer and update local pointer
        ::_tcscpy_s(const_cast<wchar_t*>(fname), _MAX_PATH, tfname.data());
        fname = tfname.data();
    }

    if (!defaultOption)
    {
        ::GetPrivateProfileString(_T("connection"), _T("host"), _T(""), m_host.data(), static_cast<DWORD>(m_host.size()), fname);
        if ((m_port = ::GetPrivateProfileInt(_T("connection"), _T("port"), 0, fname)) == 0)
        {
            return -1;
        }
    }
    else
    {
        m_host[0U] = _T('\0');
        m_port = -1;
    }
    ::GetPrivateProfileString(_T("connection"), _T("proxyhost"), _T(""), m_proxyhost.data(), static_cast<DWORD>(m_proxyhost.size()), fname);
    m_proxyport = ::GetPrivateProfileInt(_T("connection"), _T("proxyport"), 0, fname);
    m_connectionType = (ConnectionType)::GetPrivateProfileInt(_T("options"), _T("UseProxy"), DIRECT_TCP, fname);

    // Password stored as hex string, use Unicode API for correct filename handling
    std::array<wchar_t, 32U> bufW;
    m_encPasswd[0U] = '\0';
    if (::GetPrivateProfileStringW(L"connection", L"password", L"", bufW.data(), 32, fname) > 0)
    {
        std::array<char, 32U> buf;
        ::WideCharToMultiByte(CP_UTF8, 0, bufW.data(), -1, buf.data(), 32, nullptr, nullptr);
        for (int i{ 0 }; i < MAXPWLEN; ++i)
        {
            int x{ 0 };
            sscanf_s(buf.data() + i * 2, "%2x", &x);
            m_encPasswd[i] = static_cast<unsigned char>(x);
        }
    }

    if (fFromDialog)
    {
        m_opts->LoadOptions(fname);
    }
    else
    {
        if ((::_tcslen(m_host.data()) == 0) || (::_tcscmp(fname, m_opts->getDefaultOptionsFileName()) == 0))
        {
            // Load the rest of params
            ::_tcscpy_s(m_opts->m_proxyhost.data(), m_opts->m_proxyhost.size(), m_proxyhost.data());
            m_opts->m_proxyport = m_proxyport;
            m_opts->m_connectionType = m_connectionType;
            m_opts->LoadOptions(fname);
            //m_opts->Register();
            // Then display the session dialog to get missing params again
            SessionDialog sessdlg(m_opts, this, m_pDSMPlugin); //sf@2002
            if (!sessdlg.DoDialog())
            {
                QuietException_helper(L"");
            }
            ::_tcsncpy_s(m_host.data(), m_host.size(), sessdlg.m_host_dialog.data(), _TRUNCATE);
            m_port = sessdlg.m_port;
            ::_tcsncpy_s(m_proxyhost.data(), m_proxyhost.size(), sessdlg.m_proxyhost.data(), _TRUNCATE);
            m_proxyport = sessdlg.m_proxyport;
            m_connectionType = sessdlg.m_connectionType;
        }
        else if (config_specified)
        {
            ::_tcscpy_s(m_opts->m_proxyhost.data(), m_opts->m_proxyhost.size(), m_proxyhost.data());
            m_opts->m_proxyport = m_proxyport;
            m_opts->m_connectionType = m_connectionType;
            m_opts->LoadOptions(fname);
        }
    }
    return 0;
}
