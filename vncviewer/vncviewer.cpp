// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// Suppress deprecated API warnings for legacy compatibility
#pragma warning(disable : 4996)

#include "stdhdrs.h"

#include "vncviewer.h"

#include "display.h"
#include "Exception.h"

#include <shlobj.h>

#include <array>
#include <cstddef>
#include <cstring>

/*#ifdef _INTERNALLIB
#pragma comment(lib, "zlibstat.lib")
#pragma comment(lib, "zip32.lib")
#pragma comment(lib, "unz32lib.lib")
#pragma comment(lib, "libjpeg-turbo-win-static.lib")
#pragma comment(lib, "libzstd_static.lib")
#endif*/

#include "omnithread/omnithread.h"
#include "shellscalingapi.h"
#include "UltraVNCHelperFunctions.h"
#include "VNCviewerApp32.h"
// All logging is done via the log object

Log vnclog;
std::array<wchar_t, 64U> sz_A1;
std::array<wchar_t, 64U> sz_A2;
std::array<wchar_t, 64U> sz_A3;
std::array<wchar_t, 64U> sz_A4;
std::array<wchar_t, 64U> sz_A5;
std::array<wchar_t, 64U> sz_B1;
std::array<wchar_t, 64U> sz_B2;
std::array<wchar_t, 64U> sz_B3;
wchar_t sz_C1[64U];
wchar_t sz_C2[64U];
wchar_t sz_C3[64U];
std::array<wchar_t, 64U> sz_D1;
std::array<wchar_t, 64U> sz_D2;
std::array<wchar_t, 64U> sz_D3;
std::array<wchar_t, 64U> sz_D4;
std::array<wchar_t, 64U> sz_D5;
std::array<wchar_t, 64U> sz_D6;
std::array<wchar_t, 64U> sz_D7;
std::array<wchar_t, 64U> sz_D8;
std::array<wchar_t, 64U> sz_D9;
std::array<wchar_t, 64U> sz_D10;
std::array<wchar_t, 64U> sz_D11;
std::array<wchar_t, 64U> sz_D12;
std::array<wchar_t, 64U> sz_D13;
std::array<wchar_t, 64U> sz_D14;
std::array<wchar_t, 64U> sz_D15;
std::array<wchar_t, 64U> sz_D16;
std::array<wchar_t, 64U> sz_D17;
std::array<wchar_t, 64U> sz_D18;
std::array<wchar_t, 64U> sz_D19;
std::array<wchar_t, 64U> sz_D20;
std::array<wchar_t, 64U> sz_D21;
std::array<wchar_t, 64U> sz_D22;
std::array<wchar_t, 64U> sz_D23;
std::array<wchar_t, 64U> sz_D24;
std::array<wchar_t, 64U> sz_D25;
std::array<wchar_t, 64U> sz_D26;
std::array<wchar_t, 64U> sz_D27;
std::array<wchar_t, 64U> sz_D28;
std::array<wchar_t, 64U> sz_E1;
std::array<wchar_t, 64U> sz_E2;
std::array<wchar_t, 64U> sz_F1;
std::array<wchar_t, 64U> sz_F3;
std::array<wchar_t, 64U> sz_F4;
std::array<wchar_t, 128U> sz_F5;
std::array<wchar_t, 64U> sz_F6;
std::array<wchar_t, 128U> sz_F7;
std::array<wchar_t, 128U> sz_F8;
std::array<wchar_t, 64U> sz_F10;
std::array<wchar_t, 64U> sz_F11;
std::array<wchar_t, 64U> sz_G1;
std::array<wchar_t, 64U> sz_G2;
std::array<wchar_t, 64U> sz_G3;
std::array<wchar_t, 64U> sz_H1;
std::array<wchar_t, 64U> sz_H2;
std::array<wchar_t, 128U> sz_H3;
std::array<wchar_t, 64U> sz_H4;
std::array<wchar_t, 64U> sz_H5;
std::array<wchar_t, 64U> sz_H6;
std::array<wchar_t, 64U> sz_H7;
std::array<wchar_t, 64U> sz_H8;
std::array<wchar_t, 64U> sz_H9;
std::array<wchar_t, 64U> sz_H10;
std::array<wchar_t, 64U> sz_H11;
std::array<wchar_t, 64U> sz_H12;
std::array<wchar_t, 64U> sz_H13;
std::array<wchar_t, 64U> sz_H14;
std::array<wchar_t, 64U> sz_H15;
std::array<wchar_t, 64U> sz_H16;
std::array<wchar_t, 64U> sz_H17;
std::array<wchar_t, 64U> sz_H18;
std::array<wchar_t, 64U> sz_H19;
std::array<wchar_t, 64U> sz_H20;
std::array<wchar_t, 64U> sz_H21;
std::array<wchar_t, 64U> sz_H22;
std::array<wchar_t, 64U> sz_H23;
std::array<wchar_t, 64U> sz_H24;
std::array<wchar_t, 64U> sz_H25;
std::array<wchar_t, 64U> sz_H26;
std::array<wchar_t, 64U> sz_H27;
std::array<wchar_t, 64U> sz_H28;
std::array<wchar_t, 64U> sz_H29;
std::array<wchar_t, 64U> sz_H30;
std::array<wchar_t, 64U> sz_H31;
std::array<wchar_t, 64U> sz_H32;
std::array<wchar_t, 64U> sz_H33;
std::array<wchar_t, 64U> sz_H34;
std::array<wchar_t, 64U> sz_H35;
std::array<wchar_t, 64U> sz_H36;
std::array<wchar_t, 64U> sz_H37;
std::array<wchar_t, 128U> sz_H38;
std::array<wchar_t, 64U> sz_H39;
std::array<wchar_t, 64U> sz_H40;
std::array<wchar_t, 64U> sz_H41;
std::array<wchar_t, 64U> sz_H42;
std::array<wchar_t, 128U> sz_H43;
std::array<wchar_t, 64U> sz_H44;
std::array<wchar_t, 64U> sz_H45;
std::array<wchar_t, 128U> sz_H46;
std::array<wchar_t, 64U> sz_H47;
std::array<wchar_t, 64U> sz_H48;
std::array<wchar_t, 64U> sz_H49;
std::array<wchar_t, 64U> sz_H50;
std::array<wchar_t, 64U> sz_H51;
std::array<wchar_t, 64U> sz_H52;
std::array<wchar_t, 64U> sz_H53;
std::array<wchar_t, 64U> sz_H54;
std::array<wchar_t, 64U> sz_H55;
std::array<wchar_t, 64U> sz_H56;
std::array<wchar_t, 64U> sz_H57;

std::array<wchar_t, 64U> sz_H58;
std::array<wchar_t, 64U> sz_H59;
std::array<wchar_t, 64U> sz_H60;
std::array<wchar_t, 64U> sz_H61;
std::array<wchar_t, 128U> sz_H62;
std::array<wchar_t, 64U> sz_H63;
std::array<wchar_t, 64U> sz_H64;
std::array<wchar_t, 64U> sz_H65;
std::array<wchar_t, 64U> sz_H66;
std::array<wchar_t, 64U> sz_H67;
std::array<wchar_t, 128U> sz_H68;
std::array<wchar_t, 64U> sz_H69;
std::array<wchar_t, 64U> sz_H70;
std::array<wchar_t, 64U> sz_H71;
std::array<wchar_t, 128U> sz_H72;
std::array<wchar_t, 64U> sz_H73;

std::array<wchar_t, 64U> sz_I1;
std::array<wchar_t, 64U> sz_I2;
std::array<wchar_t, 64U> sz_I3;
std::array<wchar_t, 64U> sz_I4;
wchar_t sz_J1[128U];
wchar_t sz_J2[64U];
std::array<wchar_t, 64U> sz_K1;
std::array<wchar_t, 64U> sz_K2;
std::array<wchar_t, 128U> sz_K3;
std::array<wchar_t, 64U> sz_K4;
std::array<wchar_t, 64U> sz_K5;
std::array<wchar_t, 64U> sz_K6;
std::array<wchar_t, 64U> sz_K7;
std::array<wchar_t, 64U> sz_L1;
std::array<wchar_t, 64U> sz_L2;
std::array<wchar_t, 64U> sz_L3;
std::array<wchar_t, 64U> sz_L4;
std::array<wchar_t, 64U> sz_L5;
std::array<wchar_t, 64U> sz_L6;
std::array<wchar_t, 64U> sz_L7;
std::array<wchar_t, 64U> sz_L8;
std::array<wchar_t, 64U> sz_L9;
std::array<wchar_t, 64U> sz_L10;
std::array<wchar_t, 64U> sz_L11;
std::array<wchar_t, 64U> sz_L12;
std::array<wchar_t, 64U> sz_L13;
std::array<wchar_t, 64U> sz_L14;
std::array<wchar_t, 64U> sz_L15;
std::array<wchar_t, 64U> sz_L16;
std::array<wchar_t, 64U> sz_L17;
std::array<wchar_t, 64U> sz_L18;
std::array<wchar_t, 64U> sz_L19;
std::array<wchar_t, 64U> sz_L20;
std::array<wchar_t, 64U> sz_L21;
std::array<wchar_t, 64U> sz_L22;
std::array<wchar_t, 64U> sz_L23;
std::array<wchar_t, 64U> sz_L24;
std::array<wchar_t, 64U> sz_L25;
std::array<wchar_t, 64U> sz_L26;
std::array<wchar_t, 64U> sz_L27;
std::array<wchar_t, 64U> sz_L28;
std::array<wchar_t, 64U> sz_L29;
std::array<wchar_t, 64U> sz_L30;
std::array<wchar_t, 64U> sz_L31;
std::array<wchar_t, 64U> sz_L32;
std::array<wchar_t, 64U> sz_L33;
std::array<wchar_t, 64U> sz_L34;
std::array<wchar_t, 64U> sz_L35;
std::array<wchar_t, 64U> sz_L36;
std::array<wchar_t, 64U> sz_L37;
std::array<wchar_t, 64U> sz_L38;
std::array<wchar_t, 64U> sz_L39;
std::array<wchar_t, 64U> sz_L40;
std::array<wchar_t, 64U> sz_L41;
std::array<wchar_t, 64U> sz_L42;
std::array<wchar_t, 64U> sz_L43;
std::array<wchar_t, 64U> sz_L44;
std::array<wchar_t, 64U> sz_L45;
std::array<wchar_t, 64U> sz_L46;
std::array<wchar_t, 64U> sz_L47;
std::array<wchar_t, 64U> sz_L48;
std::array<wchar_t, 64U> sz_L49;
std::array<wchar_t, 64U> sz_L50;
std::array<wchar_t, 128U> sz_L51;
std::array<wchar_t, 64U> sz_L52;
std::array<wchar_t, 64U> sz_L53;
std::array<wchar_t, 64U> sz_L54;
std::array<wchar_t, 64U> sz_L55;
std::array<wchar_t, 64U> sz_L56;
std::array<wchar_t, 64U> sz_L57;
std::array<wchar_t, 64U> sz_L58;
std::array<wchar_t, 64U> sz_L59;
std::array<wchar_t, 64U> sz_L60;
std::array<wchar_t, 64U> sz_L61;
std::array<wchar_t, 64U> sz_L62;
std::array<wchar_t, 64U> sz_L63;
std::array<wchar_t, 64U> sz_L64;
std::array<wchar_t, 64U> sz_L65;
std::array<wchar_t, 64U> sz_L66;
std::array<wchar_t, 64U> sz_L67;
std::array<wchar_t, 64U> sz_L68;
std::array<wchar_t, 64U> sz_L69;
std::array<wchar_t, 64U> sz_L70;
std::array<wchar_t, 64U> sz_L71;
std::array<wchar_t, 64U> sz_L72;
std::array<wchar_t, 64U> sz_L73;
std::array<wchar_t, 64U> sz_L74;
std::array<wchar_t, 64U> sz_L75;
std::array<wchar_t, 64U> sz_L76;
std::array<wchar_t, 128U> sz_L77;
std::array<wchar_t, 64U> sz_L78;
std::array<wchar_t, 64U> sz_L79;
std::array<wchar_t, 64U> sz_L80;
std::array<wchar_t, 128U> sz_L81;
std::array<wchar_t, 64U> sz_L82;
std::array<wchar_t, 64U> sz_L83;
std::array<wchar_t, 64U> sz_L84;
std::array<wchar_t, 64U> sz_L85;
std::array<wchar_t, 64U> sz_L86;
std::array<wchar_t, 64U> sz_L87;
std::array<wchar_t, 64U> sz_L88;
std::array<wchar_t, 64U> sz_L89;
std::array<wchar_t, 64U> sz_L90;
std::array<wchar_t, 64U> sz_L91;
std::array<wchar_t, 64U> sz_L92;
std::array<wchar_t, 64U> sz_L93;
std::array<wchar_t, 64U> sz_L94;
std::array<TCHAR, 64U> sz_ShowOptions;
std::array<TCHAR, 64U> sz_HideOptions;
std::array<TCHAR, 64U> sz_Computer;
std::array<TCHAR, 64U> sz_ID;
std::array<TCHAR, 64U> sz_Port;

// 14 April 2008 jdp
std::array<wchar_t, 64U> sz_H94;
std::array<wchar_t, 64U> sz_H95;
std::array<wchar_t, 64U> sz_H96;
std::array<wchar_t, 64U> sz_H97;
std::array<wchar_t, 64U> sz_H98;
std::array<wchar_t, 64U> sz_H99;
std::array<wchar_t, 64U> sz_H100;
std::array<wchar_t, 64U> sz_H101;
std::array<wchar_t, 128U> sz_H102;
// File/dir Rename messages
std::array<wchar_t, 64U> sz_M1;
std::array<wchar_t, 64U> sz_M2;
std::array<wchar_t, 64U> sz_M3;
std::array<wchar_t, 64U> sz_M4;
std::array<wchar_t, 64U> sz_M5;
std::array<wchar_t, 64U> sz_M6;
std::array<wchar_t, 64U> sz_M7;
std::array<wchar_t, 64U> sz_M8;
std::array<wchar_t, 64U> sz_N1;
std::array<wchar_t, 64U> sz_N2;
std::array<wchar_t, 64U> sz_N3;
std::array<wchar_t, 64U> sz_N4;
std::array<wchar_t, 64U> sz_N5;
std::array<wchar_t, 64U> sz_N6;
std::array<wchar_t, 64U> sz_N7;
std::array<wchar_t, 64U> sz_N8;
std::array<wchar_t, 64U> sz_N9;
std::array<wchar_t, 64U> sz_N10;
std::array<wchar_t, 64U> sz_N11;
std::array<wchar_t, 64U> sz_N12;
std::array<wchar_t, 64U> sz_N13;
std::array<wchar_t, 64U> sz_N14;


bool command_line{ true };
bool g_passwordfailed{ true };
bool g_ConnectionLossAlreadyReported{ false };


// Accelerator Keys
AccelKeys TheAccelKeys;
HINSTANCE m_hInstResDLL;
HINSTANCE hInstance;


typedef void(CALLBACK* LPFNSETDLLDIRECTORY)(LPCTSTR);
static LPFNSETDLLDIRECTORY MySetDllDirectory = nullptr;

static BOOL read_reg_string(const HKEY key, const LPCTSTR sub_key, const LPCTSTR val_name, const LPBYTE data,
                            const LPDWORD data_len)
{
    HKEY hkey;
    BOOL ret = FALSE;
    int retv;

    if (ERROR_SUCCESS == ::RegOpenKeyEx(key, sub_key, 0, KEY_QUERY_VALUE, &hkey))
    {
        if (ERROR_SUCCESS == (retv = RegQueryValueEx(hkey, val_name, 0, nullptr, data, data_len)))
        {
            ret = TRUE;
        }
        else
        {
            vnclog.Print(3, _T("Could not read reg key '%s' subkey '%s' value: '%s'\nError: %u\n"),
                         ((key == HKEY_LOCAL_MACHINE)  ? _T("HKLM")
                          : (key == HKEY_CURRENT_USER) ? _T("HKCU")
                                                       : _T("???")),
                         sub_key, val_name, static_cast<UINT>(::GetLastError()));
        }
        ::RegCloseKey(key);
    }
    else
    {
        vnclog.Print(3, _T("Could not open reg subkey: %s\nError: %u\n"), sub_key, static_cast<UINT>(::GetLastError()));
    }

    return ret;
}

//#define CRASHRPT
#ifdef CRASHRPT
#ifndef _X64
#include "C:/DATA/crash/crashrpt/include/crashrpt.h"

#pragma comment(lib, "C:/DATA/crash/crashrpt/lib/CrashRpt1403")
#else
#include "C:/DATA/crash/crashrpt/include/crashrpt.h"

#include <cwchar>

#pragma comment(lib, "C:/DATA/crash/crashrpt/lib/x64/CrashRpt1403")
#endif
#endif

int WINAPI wWinMain(HINSTANCE hInstance, const HINSTANCE hPrevInstance, const PWSTR szCmdLine, const int nCmdShow)
{
    HMODULE hUser32 = ::LoadLibraryEx(_T("user32.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    HMODULE shcoreDLL = ::LoadLibraryExW(_T("SHCORE.DLL"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    //Min  Vista
    typedef BOOL(WINAPI * SetProcessDPIAwareFunc)();
    SetProcessDPIAwareFunc setDPIAwareF = nullptr;
    //Min Windows 8.1
    typedef HRESULT(WINAPI * SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);
    SetProcessDpiAwarenessFunc setDPIpiAwarenessF = nullptr;
    //Min Windows 10, version 1703
    typedef HRESULT(WINAPI * SetProcessDpiAwarenessContextFunc)(DPI_AWARENESS_CONTEXT);
    SetProcessDpiAwarenessContextFunc SetProcessDpiAwarenessContextF = nullptr;
    if (hUser32)
    {
        setDPIAwareF = (SetProcessDPIAwareFunc)::GetProcAddress(hUser32, "SetProcessDPIAware");
        SetProcessDpiAwarenessContextF =
            (SetProcessDpiAwarenessContextFunc)::GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
    }
    if (shcoreDLL)
    {
        setDPIpiAwarenessF = (SetProcessDpiAwarenessFunc)::GetProcAddress(shcoreDLL, "SetProcessDpiAwareness");
    }

    HRESULT hr = S_FALSE;
    if (SetProcessDpiAwarenessContextF)
    {
        hr = SetProcessDpiAwarenessContextF(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }
    if ((hr != S_OK) && setDPIpiAwarenessF)
    {
        hr = setDPIpiAwarenessF(PROCESS_PER_MONITOR_DPI_AWARE);
    }
    if ((hr != S_OK) && (setDPIAwareF))
    {
        setDPIAwareF();
    }

    if (hUser32)
    {
        ::FreeLibrary(hUser32);
    }
    if (shcoreDLL)
    {
        ::FreeLibrary(shcoreDLL);
    }

#ifdef CRASHRPT
    CR_INSTALL_INFO info;
    std::memset(&info, 0, sizeof(CR_INSTALL_INFO));
    info.cb = sizeof(CR_INSTALL_INFO);
    info.pszAppName = _T("UVNC");
    info.pszAppVersion = _T("1.4.4.0-dev");
    info.pszEmailSubject = _T("UltraVNC Viewer 1.4.4.0-dev Error Report");
    info.pszEmailTo = _T("uvnc@skynet.be");
    info.uPriorities[CR_SMAPI] = 1; // Third try send report over Simple MAPI
    // Install all available exception handlers
    info.dwFlags |= CR_INST_ALL_POSSIBLE_HANDLERS;
    // Restart the app on crash
    info.dwFlags |= CR_INST_APP_RESTART;
    info.dwFlags |= CR_INST_SEND_QUEUED_REPORTS;
    info.dwFlags |= CR_INST_AUTO_THREAD_HANDLERS;
    info.pszRestartCmdLine = _T("/restart");
    // Define the Privacy Policy URL

    // Install crash reporting
    int nResult = crInstall(&info);
    if (nResult != 0)
    {
        // Something goes wrong. Get error message.
        std::array<TCHAR, 512U> szErrorMsg{};
        crGetLastErrorMsg(szErrorMsg.data(), 512);
        _tprintf_s(_T("%s\n"), szErrorMsg.data());
        return 1;
    }
#endif

    setbuf(stderr, 0);
    bool console{ false };
    ::hInstance = hInstance;
    m_hInstResDLL = nullptr;

    // [v1.0.2-jp1 fix]
    //m_hInstResDLL = LoadLibrary("lang.dll");
    HMODULE hmod;
    HKEY hkey;
    if ((hmod = ::GetModuleHandleW(_T("kernel32.dll"))))
    {
        MySetDllDirectory = (LPFNSETDLLDIRECTORY)::GetProcAddress(hmod, "SetDllDirectoryW");
        if (MySetDllDirectory)
        {
            ::MySetDllDirectory(_T(""));
        }
        else
        {
            OSVERSIONINFO osinfo;
            osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            GetVersionEx(&osinfo);
            if (((osinfo.dwMajorVersion == 5) && (osinfo.dwMinorVersion == 0) &&
                 (::_tcsicmp((TCHAR*)osinfo.szCSDVersion, _T("Service Pack 3")) >= 0)) ||
                ((osinfo.dwMajorVersion == 5) && (osinfo.dwMinorVersion == 1) &&
                 (::_tcsicmp((TCHAR*)osinfo.szCSDVersion, _T("")) >= 0)))
            {
                DWORD regval{ 1U };
                DWORD reglen = sizeof(DWORD);

                vnclog.Print(3, _T("Using Win2k (SP3+) / WinXP (No SP).. Checking SafeDllSearch\n"));
                read_reg_string(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Session Manager"),
                                _T("SafeDllSearchMode"), (LPBYTE)&regval, &reglen);

                if (regval != 0)
                {
                    vnclog.Print(3, _T("Trying to set SafeDllSearchMode to 0\n"));
                    regval = 0;
                    if (::RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("System\\CurrentControlSet\\Control\\Session Manager"), 0,
                                     KEY_SET_VALUE, &hkey) == ERROR_SUCCESS)
                    {
                        if (::RegSetValueEx(hkey, _T("SafeDllSearchMode"), 0, REG_DWORD, (LPBYTE)&regval, sizeof(DWORD)) !=
                            ERROR_SUCCESS)
                        {
                            vnclog.Print(3, _T("Error writing SafeDllSearchMode. Error: %u\n"), static_cast<UINT>(::GetLastError()));
                        }
                        ::RegCloseKey(hkey);
                    }
                    else
                    {
                        vnclog.Print(3, _T("Error opening Session Manager key for writing. Error: %u\n"),
                                     static_cast<UINT>(::GetLastError()));
                    }
                }
                else
                {
                    vnclog.Print(3, _T("SafeDllSearchMode is set to 0\n"));
                }
            }
        }
    }

    //limit the vnclang.dll searchpath to avoid
    std::array<wchar_t, MAX_PATH> szCurrentDir{};
    std::array<wchar_t, MAX_PATH> szCurrentDir_vnclangdll{};
    if (::GetModuleFileNameW(nullptr, szCurrentDir.data(), szCurrentDir.size()))
    {
        wchar_t* p = std::wcsrchr(szCurrentDir.data(), L'\\');
        *p = L'\0';
    }

    // Load saved language preference from options.vnc
    std::array<wchar_t, MAX_PATH> optionsFile;
    std::array<wchar_t, 32U> savedLanguage{L"en"}; // Default to English

    // Try to find and load options file
    ::wcscpy_s(optionsFile.data(), optionsFile.size(), szCurrentDir.data());
    ::wcscat_s(optionsFile.data(), optionsFile.size(), L"\\options.vnc");

    // Check if file exists and read language setting
    HANDLE hFile =
        ::CreateFileW(optionsFile.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        // Try AppData folder
        if (::SHGetFolderPathW(0, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, optionsFile.data()) == S_OK)
        {
            ::wcscat_s(optionsFile.data(), optionsFile.size(), L"\\UltraVNC\\");
            ::wcscat_s(optionsFile.data(), optionsFile.size(), L"options.vnc");
            hFile = ::CreateFileW(optionsFile.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
        }
    }

    if (hFile != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(hFile);
        // Read language from INI file using GetPrivateProfileString
        ::GetPrivateProfileStringW(L"options", L"language", L"en", savedLanguage.data(), savedLanguage.size(), optionsFile.data());
    }

    // Load appropriate language DLL based on saved preference
    m_hInstResDLL = hInstance; // Default to English

    if (::_wcsicmp(savedLanguage.data(), L"en") != 0)
    {
        // Try to load language DLL
        ::swprintf_s(szCurrentDir_vnclangdll.data(), szCurrentDir_vnclangdll.size(), L"%s\\languages\\vnclang_%s.dll", szCurrentDir.data(),
                   savedLanguage.data());
        HMODULE hLangDLL = ::LoadLibraryW(szCurrentDir_vnclangdll.data());

        if (hLangDLL)
        {
            m_hInstResDLL = hLangDLL;
        }
        else
        {
            // Fallback: try root folder
            ::swprintf_s(szCurrentDir_vnclangdll.data(), szCurrentDir_vnclangdll.size(), L"%s\\vnclang_%s.dll", szCurrentDir.data(),
                       savedLanguage.data());
            hLangDLL = ::LoadLibraryW(szCurrentDir_vnclangdll.data());
            if (hLangDLL)
            {
                m_hInstResDLL = hLangDLL;
            }
        }
    }

    if (::_tcscmp(szCmdLine, _T("")) == 0)
    {
        command_line = false;
    }
    ::LoadStringW(m_hInstResDLL, IDS_A1, sz_A1.data(), sz_A1.size());
    ::LoadStringW(m_hInstResDLL, IDS_A2, sz_A2.data(), sz_A2.size());
    ::LoadStringW(m_hInstResDLL, IDS_A3, sz_A3.data(), sz_A3.size());
    ::LoadStringW(m_hInstResDLL, IDS_A4, sz_A4.data(), sz_A4.size());
    ::LoadStringW(m_hInstResDLL, IDS_A5, sz_A5.data(), sz_A5.size());
    ::LoadStringW(m_hInstResDLL, IDS_B1, sz_B1.data(), sz_B1.size());
    ::LoadStringW(m_hInstResDLL, IDS_B2, sz_B2.data(), sz_B2.size());
    ::LoadStringW(m_hInstResDLL, IDS_B3, sz_B3.data(), sz_B3.size());
    ::LoadStringW(m_hInstResDLL, IDS_C1, sz_C1, _countof(sz_C1));
    ::LoadStringW(m_hInstResDLL, IDS_C2, sz_C2, _countof(sz_C2));
    ::LoadStringW(m_hInstResDLL, IDS_C3, sz_C3, _countof(sz_C3));
    ::LoadStringW(m_hInstResDLL, IDS_D1, sz_D1.data(), sz_D1.size());
    ::LoadStringW(m_hInstResDLL, IDS_D2, sz_D2.data(), sz_D2.size());
    ::LoadStringW(m_hInstResDLL, IDS_D3, sz_D3.data(), sz_D3.size());
    ::LoadStringW(m_hInstResDLL, IDS_D4, sz_D4.data(), sz_D4.size());
    ::LoadStringW(m_hInstResDLL, IDS_D5, sz_D5.data(), sz_D5.size());
    ::LoadStringW(m_hInstResDLL, IDS_D6, sz_D6.data(), sz_D6.size());
    ::LoadStringW(m_hInstResDLL, IDS_D7, sz_D7.data(), sz_D7.size());
    ::LoadStringW(m_hInstResDLL, IDS_D8, sz_D8.data(), sz_D8.size());
    ::LoadStringW(m_hInstResDLL, IDS_D9, sz_D9.data(), sz_D9.size());
    ::LoadStringW(m_hInstResDLL, IDS_D10, sz_D10.data(), sz_D10.size());
    ::LoadStringW(m_hInstResDLL, IDS_D11, sz_D11.data(), sz_D11.size());
    ::LoadStringW(m_hInstResDLL, IDS_D12, sz_D12.data(), sz_D12.size());
    ::LoadStringW(m_hInstResDLL, IDS_D13, sz_D13.data(), sz_D13.size());
    ::LoadStringW(m_hInstResDLL, IDS_D14, sz_D14.data(), sz_D14.size());
    ::LoadStringW(m_hInstResDLL, IDS_D15, sz_D15.data(), sz_D15.size());
    ::LoadStringW(m_hInstResDLL, IDS_D16, sz_D16.data(), sz_D16.size());
    ::LoadStringW(m_hInstResDLL, IDS_D17, sz_D17.data(), sz_D17.size());
    ::LoadStringW(m_hInstResDLL, IDS_D18, sz_D18.data(), sz_D18.size());
    ::LoadStringW(m_hInstResDLL, IDS_D19, sz_D19.data(), sz_D19.size());
    ::LoadStringW(m_hInstResDLL, IDS_D20, sz_D20.data(), sz_D20.size());
    ::LoadStringW(m_hInstResDLL, IDS_D21, sz_D21.data(), sz_D21.size());
    ::LoadStringW(m_hInstResDLL, IDS_D22, sz_D22.data(), sz_D22.size());
    ::LoadStringW(m_hInstResDLL, IDS_D23, sz_D23.data(), sz_D23.size());
    ::LoadStringW(m_hInstResDLL, IDS_D24, sz_D24.data(), sz_D24.size());
    ::LoadStringW(m_hInstResDLL, IDS_D25, sz_D25.data(), sz_D25.size());
    ::LoadStringW(m_hInstResDLL, IDS_D26, sz_D26.data(), sz_D26.size());
    ::LoadStringW(m_hInstResDLL, IDS_D27, sz_D27.data(), sz_D27.size());
    ::LoadStringW(m_hInstResDLL, IDS_D28, sz_D28.data(), sz_D28.size());
    ::LoadStringW(m_hInstResDLL, IDS_E1, sz_E1.data(), sz_E1.size());
    ::LoadStringW(m_hInstResDLL, IDS_E2, sz_E2.data(), sz_E2.size());
    ::LoadStringW(m_hInstResDLL, IDS_F1, sz_F1.data(), sz_F1.size());
    ::LoadStringW(m_hInstResDLL, IDS_F3, sz_F3.data(), sz_F3.size());
    ::LoadStringW(m_hInstResDLL, IDS_F4, sz_F4.data(), sz_F4.size());
    ::LoadStringW(m_hInstResDLL, IDS_F5, sz_F5.data(), sz_F5.size());
    ::LoadStringW(m_hInstResDLL, IDS_F6, sz_F6.data(), sz_F6.size());
    ::LoadStringW(m_hInstResDLL, IDS_F7, sz_F7.data(), sz_F7.size());
    ::LoadStringW(m_hInstResDLL, IDS_F8, sz_F8.data(), sz_F8.size());
    ::LoadStringW(m_hInstResDLL, IDS_F10, sz_F10.data(), sz_F10.size());
    ::LoadStringW(m_hInstResDLL, IDS_F11, sz_F11.data(), sz_F11.size());
    ::LoadStringW(m_hInstResDLL, IDS_G1, sz_G1.data(), sz_G1.size());
    ::LoadStringW(m_hInstResDLL, IDS_G1, sz_G2.data(), sz_G2.size());
    ::LoadStringW(m_hInstResDLL, IDS_G1, sz_G3.data(), sz_G3.size());

    ::LoadStringW(m_hInstResDLL, IDS_H1, sz_H1.data(), sz_H1.size());
    ::LoadStringW(m_hInstResDLL, IDS_H2, sz_H2.data(), sz_H2.size());
    ::LoadStringW(m_hInstResDLL, IDS_H3, sz_H3.data(), sz_H3.size());
    ::LoadStringW(m_hInstResDLL, IDS_H4, sz_H4.data(), sz_H4.size());
    ::LoadStringW(m_hInstResDLL, IDS_H5, sz_H5.data(), sz_H5.size());
    ::LoadStringW(m_hInstResDLL, IDS_H6, sz_H6.data(), sz_H6.size());
    ::LoadStringW(m_hInstResDLL, IDS_H7, sz_H7.data(), sz_H7.size());
    ::LoadStringW(m_hInstResDLL, IDS_H8, sz_H8.data(), sz_H8.size());
    ::LoadStringW(m_hInstResDLL, IDS_H9, sz_H9.data(), sz_H9.size());
    ::LoadStringW(m_hInstResDLL, IDS_H10, sz_H10.data(), sz_H10.size());
    ::LoadStringW(m_hInstResDLL, IDS_H11, sz_H11.data(), sz_H11.size());
    ::LoadStringW(m_hInstResDLL, IDS_H12, sz_H12.data(), sz_H12.size());
    ::LoadStringW(m_hInstResDLL, IDS_H13, sz_H13.data(), sz_H13.size());
    ::LoadStringW(m_hInstResDLL, IDS_H14, sz_H14.data(), sz_H14.size());
    ::LoadStringW(m_hInstResDLL, IDS_H15, sz_H15.data(), sz_H15.size());
    ::LoadStringW(m_hInstResDLL, IDS_H16, sz_H16.data(), sz_H16.size());
    ::LoadStringW(m_hInstResDLL, IDS_H17, sz_H17.data(), sz_H17.size());
    ::LoadStringW(m_hInstResDLL, IDS_H18, sz_H18.data(), sz_H18.size());
    ::LoadStringW(m_hInstResDLL, IDS_H19, sz_H19.data(), sz_H19.size());
    ::LoadStringW(m_hInstResDLL, IDS_H20, sz_H20.data(), sz_H20.size());
    ::LoadStringW(m_hInstResDLL, IDS_H21, sz_H21.data(), sz_H21.size());
    ::LoadStringW(m_hInstResDLL, IDS_H22, sz_H22.data(), sz_H22.size());
    ::LoadStringW(m_hInstResDLL, IDS_H23, sz_H23.data(), sz_H23.size());
    ::LoadStringW(m_hInstResDLL, IDS_H24, sz_H24.data(), sz_H24.size());
    ::LoadStringW(m_hInstResDLL, IDS_H25, sz_H25.data(), sz_H25.size());
    ::LoadStringW(m_hInstResDLL, IDS_H26, sz_H26.data(), sz_H26.size());
    ::LoadStringW(m_hInstResDLL, IDS_H27, sz_H27.data(), sz_H27.size());
    ::LoadStringW(m_hInstResDLL, IDS_H28, sz_H28.data(), sz_H28.size());
    ::LoadStringW(m_hInstResDLL, IDS_H29, sz_H29.data(), sz_H29.size());
    ::LoadStringW(m_hInstResDLL, IDS_H30, sz_H30.data(), sz_H30.size());
    ::LoadStringW(m_hInstResDLL, IDS_H31, sz_H31.data(), sz_H31.size());
    ::LoadStringW(m_hInstResDLL, IDS_H32, sz_H32.data(), sz_H32.size());
    ::LoadStringW(m_hInstResDLL, IDS_H33, sz_H33.data(), sz_H33.size());
    ::LoadStringW(m_hInstResDLL, IDS_H34, sz_H34.data(), sz_H34.size());
    ::LoadStringW(m_hInstResDLL, IDS_H35, sz_H35.data(), sz_H35.size());
    ::LoadStringW(m_hInstResDLL, IDS_H36, sz_H36.data(), sz_H36.size());
    ::LoadStringW(m_hInstResDLL, IDS_H37, sz_H37.data(), sz_H37.size());
    ::LoadStringW(m_hInstResDLL, IDS_H38, sz_H38.data(), sz_H38.size());
    ::LoadStringW(m_hInstResDLL, IDS_H39, sz_H39.data(), sz_H39.size());
    ::LoadStringW(m_hInstResDLL, IDS_H40, sz_H40.data(), sz_H40.size());
    ::LoadStringW(m_hInstResDLL, IDS_H41, sz_H41.data(), sz_H41.size());
    ::LoadStringW(m_hInstResDLL, IDS_H42, sz_H42.data(), sz_H42.size());
    ::LoadStringW(m_hInstResDLL, IDS_H43, sz_H43.data(), sz_H43.size());
    ::LoadStringW(m_hInstResDLL, IDS_H44, sz_H44.data(), sz_H44.size());
    ::LoadStringW(m_hInstResDLL, IDS_H45, sz_H45.data(), sz_H45.size());
    ::LoadStringW(m_hInstResDLL, IDS_H46, sz_H46.data(), sz_H46.size());
    ::LoadStringW(m_hInstResDLL, IDS_H47, sz_H47.data(), sz_H47.size());
    ::LoadStringW(m_hInstResDLL, IDS_H48, sz_H48.data(), sz_H48.size());
    ::LoadStringW(m_hInstResDLL, IDS_H49, sz_H49.data(), sz_H49.size());
    ::LoadStringW(m_hInstResDLL, IDS_H50, sz_H50.data(), sz_H50.size());
    ::LoadStringW(m_hInstResDLL, IDS_H51, sz_H51.data(), sz_H51.size());
    ::LoadStringW(m_hInstResDLL, IDS_H52, sz_H52.data(), sz_H52.size());
    ::LoadStringW(m_hInstResDLL, IDS_H53, sz_H53.data(), sz_H53.size());
    ::LoadStringW(m_hInstResDLL, IDS_H54, sz_H54.data(), sz_H54.size());
    ::LoadStringW(m_hInstResDLL, IDS_H55, sz_H55.data(), sz_H55.size());
    ::LoadStringW(m_hInstResDLL, IDS_H56, sz_H56.data(), sz_H56.size());
    ::LoadStringW(m_hInstResDLL, IDS_H57, sz_H57.data(), sz_H57.size());

    ::LoadStringW(m_hInstResDLL, IDS_H58, sz_H58.data(), sz_H58.size());
    ::LoadStringW(m_hInstResDLL, IDS_H59, sz_H59.data(), sz_H59.size());
    ::LoadStringW(m_hInstResDLL, IDS_H60, sz_H60.data(), sz_H60.size());
    ::LoadStringW(m_hInstResDLL, IDS_H61, sz_H61.data(), sz_H61.size());
    ::LoadStringW(m_hInstResDLL, IDS_H62, sz_H62.data(), sz_H62.size());
    ::LoadStringW(m_hInstResDLL, IDS_H63, sz_H63.data(), sz_H63.size());
    ::LoadStringW(m_hInstResDLL, IDS_H64, sz_H64.data(), sz_H64.size());
    ::LoadStringW(m_hInstResDLL, IDS_H65, sz_H65.data(), sz_H65.size());
    ::LoadStringW(m_hInstResDLL, IDS_H66, sz_H66.data(), sz_H66.size());
    ::LoadStringW(m_hInstResDLL, IDS_H67, sz_H67.data(), sz_H67.size());
    ::LoadStringW(m_hInstResDLL, IDS_H68, sz_H68.data(), sz_H68.size());
    ::LoadStringW(m_hInstResDLL, IDS_H69, sz_H69.data(), sz_H69.size());
    ::LoadStringW(m_hInstResDLL, IDS_H70, sz_H70.data(), sz_H70.size());
    ::LoadStringW(m_hInstResDLL, IDS_H71, sz_H71.data(), sz_H71.size());
    ::LoadStringW(m_hInstResDLL, IDS_H72, sz_H72.data(), sz_H72.size());
    ::LoadStringW(m_hInstResDLL, IDS_H73, sz_H73.data(), sz_H73.size());


    ::LoadStringW(m_hInstResDLL, IDS_I1, sz_I1.data(), sz_I1.size());
    ::LoadStringW(m_hInstResDLL, IDS_I2, sz_I2.data(), sz_I2.size());
    ::LoadStringW(m_hInstResDLL, IDS_I3, sz_I3.data(), sz_I3.size());

    ::LoadStringW(m_hInstResDLL, IDS_J1, sz_J1, _countof(sz_J1));
    ::LoadStringW(m_hInstResDLL, IDS_J2, sz_J2, _countof(sz_J2));

    ::LoadStringW(m_hInstResDLL, IDS_K1, sz_K1.data(), sz_K1.size());
    ::LoadStringW(m_hInstResDLL, IDS_K2, sz_K2.data(), sz_K2.size());
    ::LoadStringW(m_hInstResDLL, IDS_K3, sz_K3.data(), sizeof(sz_K3) / sizeof(sz_K3[0U]));
    ::LoadStringW(m_hInstResDLL, IDS_K4, sz_K4.data(), sz_K4.size());
    ::LoadStringW(m_hInstResDLL, IDS_K5, sz_K5.data(), sz_K5.size());
    ::LoadStringW(m_hInstResDLL, IDS_K6, sz_K6.data(), sz_K6.size());

    ::LoadStringW(m_hInstResDLL, IDS_L1, sz_L1.data(), sz_L1.size());
    ::LoadStringW(m_hInstResDLL, IDS_L2, sz_L2.data(), sz_L2.size());
    ::LoadStringW(m_hInstResDLL, IDS_L3, sz_L3.data(), sz_L3.size());
    ::LoadStringW(m_hInstResDLL, IDS_L4, sz_L4.data(), sz_L4.size());
    ::LoadStringW(m_hInstResDLL, IDS_L5, sz_L5.data(), sz_L5.size());
    ::LoadStringW(m_hInstResDLL, IDS_L6, sz_L6.data(), sz_L6.size());
    ::LoadStringW(m_hInstResDLL, IDS_L7, sz_L7.data(), sz_L7.size());
    ::LoadStringW(m_hInstResDLL, IDS_L8, sz_L8.data(), sz_L8.size());
    ::LoadStringW(m_hInstResDLL, IDS_L9, sz_L9.data(), sz_L9.size());
    ::LoadStringW(m_hInstResDLL, IDS_L10, sz_L10.data(), sz_L10.size());
    ::LoadStringW(m_hInstResDLL, IDS_L11, sz_L11.data(), sz_L11.size());
    ::LoadStringW(m_hInstResDLL, IDS_L12, sz_L12.data(), sz_L12.size());
    ::LoadStringW(m_hInstResDLL, IDS_L13, sz_L13.data(), sz_L13.size());
    ::LoadStringW(m_hInstResDLL, IDS_L14, sz_L14.data(), sz_L14.size());
    ::LoadStringW(m_hInstResDLL, IDS_L15, sz_L15.data(), sz_L15.size());
    ::LoadStringW(m_hInstResDLL, IDS_L16, sz_L16.data(), sz_L16.size());
    ::LoadStringW(m_hInstResDLL, IDS_L17, sz_L17.data(), sz_L17.size());
    ::LoadStringW(m_hInstResDLL, IDS_L18, sz_L18.data(), sz_L18.size());
    ::LoadStringW(m_hInstResDLL, IDS_L19, sz_L19.data(), sz_L19.size());
    ::LoadStringW(m_hInstResDLL, IDS_L20, sz_L20.data(), sz_L20.size());
    ::LoadStringW(m_hInstResDLL, IDS_L21, sz_L21.data(), sz_L21.size());
    ::LoadStringW(m_hInstResDLL, IDS_L22, sz_L22.data(), sz_L22.size());
    ::LoadStringW(m_hInstResDLL, IDS_L23, sz_L23.data(), sz_L23.size());
    ::LoadStringW(m_hInstResDLL, IDS_L24, sz_L24.data(), sz_L24.size());
    ::LoadStringW(m_hInstResDLL, IDS_L25, sz_L25.data(), sz_L25.size());
    ::LoadStringW(m_hInstResDLL, IDS_L26, sz_L26.data(), sz_L26.size());
    ::LoadStringW(m_hInstResDLL, IDS_L27, sz_L27.data(), sz_L27.size());
    ::LoadStringW(m_hInstResDLL, IDS_L28, sz_L28.data(), sz_L28.size());
    ::LoadStringW(m_hInstResDLL, IDS_L29, sz_L29.data(), sz_L29.size());
    ::LoadStringW(m_hInstResDLL, IDS_L30, sz_L30.data(), sz_L30.size());
    ::LoadStringW(m_hInstResDLL, IDS_L31, sz_L31.data(), sz_L31.size());
    ::LoadStringW(m_hInstResDLL, IDS_L32, sz_L32.data(), sz_L32.size());
    ::LoadStringW(m_hInstResDLL, IDS_L33, sz_L33.data(), sz_L33.size());
    ::LoadStringW(m_hInstResDLL, IDS_L34, sz_L34.data(), sz_L34.size());
    ::LoadStringW(m_hInstResDLL, IDS_L35, sz_L35.data(), sz_L35.size());
    ::LoadStringW(m_hInstResDLL, IDS_L36, sz_L36.data(), sz_L36.size());
    ::LoadStringW(m_hInstResDLL, IDS_L37, sz_L37.data(), sz_L37.size());
    ::LoadStringW(m_hInstResDLL, IDS_L38, sz_L38.data(), sz_L38.size());
    ::LoadStringW(m_hInstResDLL, IDS_L39, sz_L39.data(), sz_L39.size());
    ::LoadStringW(m_hInstResDLL, IDS_L40, sz_L40.data(), sz_L40.size());
    ::LoadStringW(m_hInstResDLL, IDS_L41, sz_L41.data(), sz_L41.size());
    ::LoadStringW(m_hInstResDLL, IDS_L42, sz_L42.data(), sz_L42.size());
    ::LoadStringW(m_hInstResDLL, IDS_L43, sz_L43.data(), sz_L43.size());
    ::LoadStringW(m_hInstResDLL, IDS_L44, sz_L44.data(), sz_L44.size());
    ::LoadStringW(m_hInstResDLL, IDS_L45, sz_L45.data(), sz_L45.size());
    ::LoadStringW(m_hInstResDLL, IDS_L46, sz_L46.data(), sz_L46.size());
    ::LoadStringW(m_hInstResDLL, IDS_L47, sz_L47.data(), sz_L47.size());
    ::LoadStringW(m_hInstResDLL, IDS_L48, sz_L48.data(), sz_L48.size());
    ::LoadStringW(m_hInstResDLL, IDS_L49, sz_L49.data(), sz_L49.size());
    ::LoadStringW(m_hInstResDLL, IDS_L50, sz_L50.data(), sz_L50.size());
    ::LoadStringW(m_hInstResDLL, IDS_L51, sz_L51.data(), sz_L51.size());
    ::LoadStringW(m_hInstResDLL, IDS_L52, sz_L52.data(), sz_L52.size());
    ::LoadStringW(m_hInstResDLL, IDS_L53, sz_L53.data(), sz_L53.size());
    ::LoadStringW(m_hInstResDLL, IDS_L54, sz_L54.data(), sz_L54.size());
    ::LoadStringW(m_hInstResDLL, IDS_L55, sz_L55.data(), sz_L55.size());
    ::LoadStringW(m_hInstResDLL, IDS_L56, sz_L56.data(), sz_L56.size());
    ::LoadStringW(m_hInstResDLL, IDS_L57, sz_L57.data(), sz_L57.size());
    ::LoadStringW(m_hInstResDLL, IDS_L58, sz_L58.data(), sz_L58.size());
    ::LoadStringW(m_hInstResDLL, IDS_L59, sz_L59.data(), sz_L59.size());
    ::LoadStringW(m_hInstResDLL, IDS_L60, sz_L60.data(), sz_L60.size());
    ::LoadStringW(m_hInstResDLL, IDS_L61, sz_L61.data(), sz_L61.size());
    ::LoadStringW(m_hInstResDLL, IDS_L62, sz_L62.data(), sz_L62.size());
    ::LoadStringW(m_hInstResDLL, IDS_L63, sz_L63.data(), sz_L63.size());
    ::LoadStringW(m_hInstResDLL, IDS_L64, sz_L64.data(), sz_L64.size());
    ::LoadStringW(m_hInstResDLL, IDS_L65, sz_L65.data(), sz_L65.size());
    ::LoadStringW(m_hInstResDLL, IDS_L66, sz_L66.data(), sz_L66.size());
    ::LoadStringW(m_hInstResDLL, IDS_L67, sz_L67.data(), sz_L67.size());
    ::LoadStringW(m_hInstResDLL, IDS_L68, sz_L68.data(), sz_L68.size());
    ::LoadStringW(m_hInstResDLL, IDS_L69, sz_L69.data(), sz_L69.size());
    ::LoadStringW(m_hInstResDLL, IDS_L70, sz_L70.data(), sz_L70.size());
    ::LoadStringW(m_hInstResDLL, IDS_L71, sz_L71.data(), sz_L71.size());
    ::LoadStringW(m_hInstResDLL, IDS_L72, sz_L72.data(), sz_L72.size());
    ::LoadStringW(m_hInstResDLL, IDS_L73, sz_L73.data(), sz_L73.size());
    ::LoadStringW(m_hInstResDLL, IDS_L74, sz_L74.data(), sz_L74.size());
    ::LoadStringW(m_hInstResDLL, IDS_L75, sz_L75.data(), sz_L75.size());
    ::LoadStringW(m_hInstResDLL, IDS_L76, sz_L76.data(), sz_L76.size());
    ::LoadStringW(m_hInstResDLL, IDS_L77, sz_L77.data(), sz_L77.size());
    ::LoadStringW(m_hInstResDLL, IDS_L78, sz_L78.data(), sz_L78.size());
    ::LoadStringW(m_hInstResDLL, IDS_L79, sz_L79.data(), sz_L79.size());
    ::LoadStringW(m_hInstResDLL, IDS_L80, sz_L80.data(), sz_L80.size());
    ::LoadStringW(m_hInstResDLL, IDS_L81, sz_L81.data(), sz_L81.size());
    ::LoadStringW(m_hInstResDLL, IDS_L82, sz_L82.data(), sz_L82.size());
    ::LoadStringW(m_hInstResDLL, IDS_L83, sz_L83.data(), sz_L83.size());
    ::LoadStringW(m_hInstResDLL, IDS_L84, sz_L84.data(), sz_L84.size());
    ::LoadStringW(m_hInstResDLL, IDS_L85, sz_L85.data(), sz_L85.size());
    ::LoadStringW(m_hInstResDLL, IDS_L86, sz_L86.data(), sz_L86.size());
    ::LoadStringW(m_hInstResDLL, IDS_L87, sz_L87.data(), sz_L87.size());
    ::LoadStringW(m_hInstResDLL, IDS_L88, sz_L88.data(), sz_L88.size());
    ::LoadStringW(m_hInstResDLL, IDS_L89, sz_L89.data(), sz_L89.size());
    ::LoadStringW(m_hInstResDLL, IDS_L90, sz_L90.data(), sz_L90.size());
    ::LoadStringW(m_hInstResDLL, IDS_L91, sz_L91.data(), sz_L91.size());
    ::LoadStringW(m_hInstResDLL, IDS_L92, sz_L92.data(), sz_L92.size());
    ::LoadStringW(m_hInstResDLL, IDS_L93, sz_L93.data(), sz_L93.size());
    ::LoadStringW(m_hInstResDLL, IDS_L94, sz_L94.data(), sz_L94.size());
    ::LoadStringW(m_hInstResDLL, IDS_L95, sz_ShowOptions.data(), sizeof(sz_ShowOptions) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L96, sz_HideOptions.data(), sizeof(sz_HideOptions) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L97, sz_Computer.data(), sizeof(sz_Computer) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L98, sz_ID.data(), sizeof(sz_ID) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L99, sz_Port.data(), sizeof(sz_Port) / sizeof(TCHAR));

    ::LoadStringW(m_hInstResDLL, IDS_M1, sz_M1.data(), sz_M1.size());
    ::LoadStringW(m_hInstResDLL, IDS_M2, sz_M2.data(), sz_M2.size());
    ::LoadStringW(m_hInstResDLL, IDS_M3, sz_M3.data(), sz_M3.size());
    ::LoadStringW(m_hInstResDLL, IDS_M4, sz_M4.data(), sz_M4.size());
    ::LoadStringW(m_hInstResDLL, IDS_M5, sz_M5.data(), sz_M5.size());
    ::LoadStringW(m_hInstResDLL, IDS_M6, sz_M6.data(), sz_M6.size());
    ::LoadStringW(m_hInstResDLL, IDS_M7, sz_M7.data(), sz_M7.size());
    ::LoadStringW(m_hInstResDLL, IDS_M8, sz_M8.data(), sz_M8.size());

    // 14 April 2008 jdp
    ::LoadStringW(m_hInstResDLL, IDS_H94, sz_H94.data(), sz_H94.size());
    ::LoadStringW(m_hInstResDLL, IDS_H95, sz_H95.data(), sz_H95.size());
    ::LoadStringW(m_hInstResDLL, IDS_H96, sz_H96.data(), sz_H96.size());
    ::LoadStringW(m_hInstResDLL, IDS_H97, sz_H97.data(), sz_H97.size());
    ::LoadStringW(m_hInstResDLL, IDS_H98, sz_H98.data(), sz_H98.size());
    ::LoadStringW(m_hInstResDLL, IDS_H99, sz_H99.data(), sz_H99.size());
    ::LoadStringW(m_hInstResDLL, IDS_H100, sz_H100.data(), sz_H100.size());
    ::LoadStringW(m_hInstResDLL, IDS_H101, sz_H101.data(), sz_H101.size());
    ::LoadStringW(m_hInstResDLL, IDS_H102, sz_H102.data(), sz_H102.size());
    //	RegisterLinkLabel(m_hInstResDLL);
    loadStrings(m_hInstResDLL);

    /////////////////////////////////////////////////////////////

    // The state of the application as a whole is contained in the one app object
    VNCviewerApp32 app(hInstance, szCmdLine);

    console = app.m_options.m_logToConsole;

    // Start a new connection if specified on command line,
    // or if not in listening mode
    MSG msg;
    while (g_passwordfailed == true)
    {
        g_passwordfailed = false;
        if (app.m_options.m_connectionSpecified && !app.m_options.m_listening)
        {
            app.NewConnection(false, app.m_options.m_host_options.data(), app.m_options.m_port);
        }
        else if (!app.m_options.m_listening)
        {
            // This one will also read from config file if specified
            app.NewConnection(false);
        }

        try
        {
            while (::GetMessage(&msg, nullptr, 0, 0))
            {
                if (!TheAccelKeys.TranslateAccelKeys(&msg))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
            }
        }
        catch (Exception& e)
        {
            if (!g_ConnectionLossAlreadyReported)
            {
                e.Report();
            }
        }
    }
    // Clean up winsock
    ::WSACleanup();
    vnclog.Print(3, _T("Exiting\n"));
#ifdef CRASHRPT
    crUninstall();
#endif
    if (console)
    {
        ::Sleep(2000);
    }
    return msg.wParam;
}


// Move the given window to the centre of the screen
// and bring it to the top.
void CentreWindow(const HWND hwnd)
{
    RECT winrect, workrect;

    // Find how large the desktop work area is
    ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workrect, 0);
    //RECT workrect;
    /*tempdisplayclass tdc;
    tdc.Init();
    workrect.left=tdc.monarray[selected_screen].wl;
    workrect.right=tdc.monarray[selected_screen].wr;
    workrect.top=tdc.monarray[selected_screen].wt;
    workrect.bottom=tdc.monarray[selected_screen].wb;*/

    int workwidth = workrect.right - workrect.left;
    int workheight = workrect.bottom - workrect.top;

    // And how big the window is
    ::GetWindowRect(hwnd, &winrect);
    int winwidth = winrect.right - winrect.left;
    int winheight = winrect.bottom - winrect.top;
    // Make sure it's not bigger than the work area
    winwidth = minimum(winwidth, workwidth);
    winheight = minimum(winheight, workheight);

    // Now centre it
    ::SetWindowPos(hwnd, HWND_TOP, workrect.left + (workwidth - winwidth) / 2, workrect.top + (workheight - winheight) / 2, winwidth,
                 winheight, SWP_SHOWWINDOW);
    ::SetForegroundWindow(hwnd);
}


// sf@2002 - TightVNC method - RealVNC method
// Convert "host:display" or "host::port" or "host:port" if port > 100, into host and port
// IPv6 addresses must use bracket notation: [2001:db8::1]:5900
// Returns true if valid format, false if not.
// Takes initial string, addresses of results and size of host buffer in wchars.
// If the display info passed in is longer than the size of the host buffer, it
// is assumed to be invalid, so false is returned.
bool ParseDisplay(const LPTSTR display, const LPTSTR phost, const int hostlen, int* pport)
{
    if (hostlen < static_cast<int>(::_tcslen(display)))
    {
        return false;
    }

    int tmp_port;
    TCHAR* colonpos = nullptr;

    // Check for IPv6 bracket notation: [IPv6]:port
    if (display[0U] == L'[')
    {
        TCHAR* bracketend = ::_tcschr(display, L']');
        if (bracketend == nullptr)
        {
            return false;
        } // Invalid format - opening bracket without closing

        // Extract IPv6 address between brackets
        std::size_t ipv6len = bracketend - display - 1;
        if (ipv6len >= static_cast<std::size_t>(hostlen))
        {
            return false;
        }

        ::_tcsncpy_s(phost, hostlen, display + 1, ipv6len);
        phost[ipv6len] = L'\0';

        // Check for port after the bracket
        if (bracketend[1U] == L'\0')
        {
            // No port specified - use default
            tmp_port = RFB_PORT_OFFSET;
        }
        else if (bracketend[1U] == L':')
        {
            if (bracketend[2U] == L':')
            {
                // [IPv6]::port format
                if (_stscanf_s(bracketend + 3, TEXT("%d"), &tmp_port) != 1)
                {
                    return false;
                }
            }
            else
            {
                // [IPv6]:display or [IPv6]:port format
                if (_stscanf_s(bracketend + 2, TEXT("%d"), &tmp_port) != 1)
                {
                    return false;
                }

                // If port < 100 interpret as display number else as Port number
                if (tmp_port < 100)
                {
                    tmp_port += RFB_PORT_OFFSET;
                }
            }
        }
        else
        {
            return false; // Invalid format after bracket
        }
    }
    else
    {
        // IPv4 or hostname parsing (original logic)
        colonpos = ::_tcschr(display, L':');
        if (colonpos == nullptr)
        {
            // No colon -- use default port number
            tmp_port = RFB_PORT_OFFSET;
            ::_tcsncpy_s(phost, MAX_HOST_NAME_LEN, display, MAX_HOST_NAME_LEN);
        }
        else
        {
            ::_tcsncpy_s(phost, MAX_HOST_NAME_LEN, display, colonpos - display);
            phost[colonpos - display] = L'\0';
            if (colonpos[1U] == L':')
            {
                // Two colons -- interpret as a port number
                if (_stscanf_s(colonpos + 2, TEXT("%d"), &tmp_port) != 1)
                {
                    return false;
                }
            }
            else
            {
                // One colon -- interpret as a display number or port number
                if (_stscanf_s(colonpos + 1, TEXT("%d"), &tmp_port) != 1)
                {
                    return false;
                }

                // RealVNC method - If port < 100 interpret as display number else as Port number
                if (tmp_port < 100)
                {
                    tmp_port += RFB_PORT_OFFSET;
                }
            }
        }
    }
    *pport = tmp_port;
    return true;
}

// Reload language DLL and all strings
void ReloadLanguage(const wchar_t* const langCode)
{
    extern HINSTANCE m_hInstResDLL;
    extern HINSTANCE hInstance;

    // Store old DLL handle
    HMODULE hOldDLL = m_hInstResDLL;

    // Build path to new language DLL
    std::array<wchar_t, MAX_PATH> exePath;
    std::array<wchar_t, MAX_PATH> langDllPath;

    if (::GetModuleFileNameW(nullptr, exePath.data(), exePath.size()))
    {
        wchar_t* lastSlash = std::wcsrchr(exePath.data(), L'\\');
        if (lastSlash)
        {
            *lastSlash = L'\0';

            // Check if English (use embedded resources)
            if (::_wcsicmp(langCode, L"en") == 0)
            {
                m_hInstResDLL = hInstance;
            }
            else
            {
                // Try to load from languages subfolder first
                ::swprintf_s(langDllPath.data(), langDllPath.size(), L"%s\\languages\\vnclang_%s.dll", exePath.data(), langCode);
                HMODULE hNewDLL = ::LoadLibraryW(langDllPath.data());

                if (hNewDLL)
                {
                    m_hInstResDLL = hNewDLL;
                }
                else
                {
                    // Fallback to root folder
                    ::swprintf_s(langDllPath.data(), langDllPath.size(), L"%s\\vnclang_%s.dll", exePath.data(), langCode);
                    hNewDLL = ::LoadLibraryW(langDllPath.data());

                    if (hNewDLL)
                    {
                        m_hInstResDLL = hNewDLL;
                    }
                    else
                    {
                        // Fallback to English
                        m_hInstResDLL = hInstance;
                    }
                }
            }
        }
    }

    // Update m_hInstResDLL to point to the new DLL
    if (pApp)
    {
        m_hInstResDLL = m_hInstResDLL;
    }

    // Unload old DLL if it was a language DLL (not the main instance)
    if ((hOldDLL != hInstance) && (hOldDLL != m_hInstResDLL))
    {
        ::FreeLibrary(hOldDLL);
    }

    // Reload all string resources
    ::LoadStringW(m_hInstResDLL, IDS_A1, sz_A1.data(), sz_A1.size());
    ::LoadStringW(m_hInstResDLL, IDS_A2, sz_A2.data(), sz_A2.size());
    ::LoadStringW(m_hInstResDLL, IDS_A3, sz_A3.data(), sz_A3.size());
    ::LoadStringW(m_hInstResDLL, IDS_A4, sz_A4.data(), sz_A4.size());
    ::LoadStringW(m_hInstResDLL, IDS_A5, sz_A5.data(), sz_A5.size());
    ::LoadStringW(m_hInstResDLL, IDS_B1, sz_B1.data(), sz_B1.size());
    ::LoadStringW(m_hInstResDLL, IDS_B2, sz_B2.data(), sz_B2.size());
    ::LoadStringW(m_hInstResDLL, IDS_B3, sz_B3.data(), sz_B3.size());
    ::LoadStringW(m_hInstResDLL, IDS_C1, sz_C1, _countof(sz_C1));
    ::LoadStringW(m_hInstResDLL, IDS_C2, sz_C2, _countof(sz_C2));
    ::LoadStringW(m_hInstResDLL, IDS_C3, sz_C3, _countof(sz_C3));
    ::LoadStringW(m_hInstResDLL, IDS_D1, sz_D1.data(), sz_D1.size());
    ::LoadStringW(m_hInstResDLL, IDS_D2, sz_D2.data(), sz_D2.size());
    ::LoadStringW(m_hInstResDLL, IDS_D3, sz_D3.data(), sz_D3.size());
    ::LoadStringW(m_hInstResDLL, IDS_D4, sz_D4.data(), sz_D4.size());
    ::LoadStringW(m_hInstResDLL, IDS_D5, sz_D5.data(), sz_D5.size());
    ::LoadStringW(m_hInstResDLL, IDS_D6, sz_D6.data(), sz_D6.size());
    ::LoadStringW(m_hInstResDLL, IDS_D7, sz_D7.data(), sz_D7.size());
    ::LoadStringW(m_hInstResDLL, IDS_D8, sz_D8.data(), sz_D8.size());
    ::LoadStringW(m_hInstResDLL, IDS_D9, sz_D9.data(), sz_D9.size());
    ::LoadStringW(m_hInstResDLL, IDS_D10, sz_D10.data(), sz_D10.size());
    ::LoadStringW(m_hInstResDLL, IDS_D11, sz_D11.data(), sz_D11.size());
    ::LoadStringW(m_hInstResDLL, IDS_D12, sz_D12.data(), sz_D12.size());
    ::LoadStringW(m_hInstResDLL, IDS_D13, sz_D13.data(), sz_D13.size());
    ::LoadStringW(m_hInstResDLL, IDS_D14, sz_D14.data(), sz_D14.size());
    ::LoadStringW(m_hInstResDLL, IDS_D15, sz_D15.data(), sz_D15.size());
    ::LoadStringW(m_hInstResDLL, IDS_D16, sz_D16.data(), sz_D16.size());
    ::LoadStringW(m_hInstResDLL, IDS_D17, sz_D17.data(), sz_D17.size());
    ::LoadStringW(m_hInstResDLL, IDS_D18, sz_D18.data(), sz_D18.size());
    ::LoadStringW(m_hInstResDLL, IDS_D19, sz_D19.data(), sz_D19.size());
    ::LoadStringW(m_hInstResDLL, IDS_D20, sz_D20.data(), sz_D20.size());
    ::LoadStringW(m_hInstResDLL, IDS_D21, sz_D21.data(), sz_D21.size());
    ::LoadStringW(m_hInstResDLL, IDS_D22, sz_D22.data(), sz_D22.size());
    ::LoadStringW(m_hInstResDLL, IDS_D23, sz_D23.data(), sz_D23.size());
    ::LoadStringW(m_hInstResDLL, IDS_D24, sz_D24.data(), sz_D24.size());
    ::LoadStringW(m_hInstResDLL, IDS_D25, sz_D25.data(), sz_D25.size());
    ::LoadStringW(m_hInstResDLL, IDS_D26, sz_D26.data(), sz_D26.size());
    ::LoadStringW(m_hInstResDLL, IDS_D27, sz_D27.data(), sz_D27.size());
    ::LoadStringW(m_hInstResDLL, IDS_D28, sz_D28.data(), sz_D28.size());
    ::LoadStringW(m_hInstResDLL, IDS_E1, sz_E1.data(), sz_E1.size());
    ::LoadStringW(m_hInstResDLL, IDS_E2, sz_E2.data(), sz_E2.size());
    ::LoadStringW(m_hInstResDLL, IDS_F1, sz_F1.data(), sz_F1.size());
    ::LoadStringW(m_hInstResDLL, IDS_F3, sz_F3.data(), sz_F3.size());
    ::LoadStringW(m_hInstResDLL, IDS_F4, sz_F4.data(), sz_F4.size());
    ::LoadStringW(m_hInstResDLL, IDS_F5, sz_F5.data(), sz_F5.size());
    ::LoadStringW(m_hInstResDLL, IDS_F6, sz_F6.data(), sz_F6.size());
    ::LoadStringW(m_hInstResDLL, IDS_F7, sz_F7.data(), sz_F7.size());
    ::LoadStringW(m_hInstResDLL, IDS_F8, sz_F8.data(), sz_F8.size());
    ::LoadStringW(m_hInstResDLL, IDS_F10, sz_F10.data(), sz_F10.size());
    ::LoadStringW(m_hInstResDLL, IDS_F11, sz_F11.data(), sz_F11.size());
    ::LoadStringW(m_hInstResDLL, IDS_G1, sz_G1.data(), sz_G1.size());
    ::LoadStringW(m_hInstResDLL, IDS_G1, sz_G2.data(), sz_G2.size());
    ::LoadStringW(m_hInstResDLL, IDS_G1, sz_G3.data(), sz_G3.size());
    ::LoadStringW(m_hInstResDLL, IDS_H1, sz_H1.data(), sz_H1.size());
    ::LoadStringW(m_hInstResDLL, IDS_H2, sz_H2.data(), sz_H2.size());
    ::LoadStringW(m_hInstResDLL, IDS_H3, sz_H3.data(), sz_H3.size());
    ::LoadStringW(m_hInstResDLL, IDS_H4, sz_H4.data(), sz_H4.size());
    ::LoadStringW(m_hInstResDLL, IDS_H5, sz_H5.data(), sz_H5.size());
    ::LoadStringW(m_hInstResDLL, IDS_H6, sz_H6.data(), sz_H6.size());
    ::LoadStringW(m_hInstResDLL, IDS_H7, sz_H7.data(), sz_H7.size());
    ::LoadStringW(m_hInstResDLL, IDS_H8, sz_H8.data(), sz_H8.size());
    ::LoadStringW(m_hInstResDLL, IDS_H9, sz_H9.data(), sz_H9.size());
    ::LoadStringW(m_hInstResDLL, IDS_H10, sz_H10.data(), sz_H10.size());
    ::LoadStringW(m_hInstResDLL, IDS_H11, sz_H11.data(), sz_H11.size());
    ::LoadStringW(m_hInstResDLL, IDS_H12, sz_H12.data(), sz_H12.size());
    ::LoadStringW(m_hInstResDLL, IDS_H13, sz_H13.data(), sz_H13.size());
    ::LoadStringW(m_hInstResDLL, IDS_H14, sz_H14.data(), sz_H14.size());
    ::LoadStringW(m_hInstResDLL, IDS_H15, sz_H15.data(), sz_H15.size());
    ::LoadStringW(m_hInstResDLL, IDS_H16, sz_H16.data(), sz_H16.size());
    ::LoadStringW(m_hInstResDLL, IDS_H17, sz_H17.data(), sz_H17.size());
    ::LoadStringW(m_hInstResDLL, IDS_H18, sz_H18.data(), sz_H18.size());
    ::LoadStringW(m_hInstResDLL, IDS_H19, sz_H19.data(), sz_H19.size());
    ::LoadStringW(m_hInstResDLL, IDS_H20, sz_H20.data(), sz_H20.size());
    ::LoadStringW(m_hInstResDLL, IDS_H21, sz_H21.data(), sz_H21.size());
    ::LoadStringW(m_hInstResDLL, IDS_H22, sz_H22.data(), sz_H22.size());
    ::LoadStringW(m_hInstResDLL, IDS_H23, sz_H23.data(), sz_H23.size());
    ::LoadStringW(m_hInstResDLL, IDS_H24, sz_H24.data(), sz_H24.size());
    ::LoadStringW(m_hInstResDLL, IDS_H25, sz_H25.data(), sz_H25.size());
    ::LoadStringW(m_hInstResDLL, IDS_H26, sz_H26.data(), sz_H26.size());
    ::LoadStringW(m_hInstResDLL, IDS_H27, sz_H27.data(), sz_H27.size());
    ::LoadStringW(m_hInstResDLL, IDS_H28, sz_H28.data(), sz_H28.size());
    ::LoadStringW(m_hInstResDLL, IDS_H29, sz_H29.data(), sz_H29.size());
    ::LoadStringW(m_hInstResDLL, IDS_H30, sz_H30.data(), sz_H30.size());
    ::LoadStringW(m_hInstResDLL, IDS_H31, sz_H31.data(), sz_H31.size());
    ::LoadStringW(m_hInstResDLL, IDS_H32, sz_H32.data(), sz_H32.size());
    ::LoadStringW(m_hInstResDLL, IDS_H33, sz_H33.data(), sz_H33.size());
    ::LoadStringW(m_hInstResDLL, IDS_H34, sz_H34.data(), sz_H34.size());
    ::LoadStringW(m_hInstResDLL, IDS_H35, sz_H35.data(), sz_H35.size());
    ::LoadStringW(m_hInstResDLL, IDS_H36, sz_H36.data(), sz_H36.size());
    ::LoadStringW(m_hInstResDLL, IDS_H37, sz_H37.data(), sz_H37.size());
    ::LoadStringW(m_hInstResDLL, IDS_H38, sz_H38.data(), sz_H38.size());
    ::LoadStringW(m_hInstResDLL, IDS_H39, sz_H39.data(), sz_H39.size());
    ::LoadStringW(m_hInstResDLL, IDS_H40, sz_H40.data(), sz_H40.size());
    ::LoadStringW(m_hInstResDLL, IDS_H41, sz_H41.data(), sz_H41.size());
    ::LoadStringW(m_hInstResDLL, IDS_H42, sz_H42.data(), sz_H42.size());
    ::LoadStringW(m_hInstResDLL, IDS_H43, sz_H43.data(), sz_H43.size());
    ::LoadStringW(m_hInstResDLL, IDS_H44, sz_H44.data(), sz_H44.size());
    ::LoadStringW(m_hInstResDLL, IDS_H45, sz_H45.data(), sz_H45.size());
    ::LoadStringW(m_hInstResDLL, IDS_H46, sz_H46.data(), sz_H46.size());
    ::LoadStringW(m_hInstResDLL, IDS_H47, sz_H47.data(), sz_H47.size());
    ::LoadStringW(m_hInstResDLL, IDS_H48, sz_H48.data(), sz_H48.size());
    ::LoadStringW(m_hInstResDLL, IDS_H49, sz_H49.data(), sz_H49.size());
    ::LoadStringW(m_hInstResDLL, IDS_H50, sz_H50.data(), sz_H50.size());
    ::LoadStringW(m_hInstResDLL, IDS_H51, sz_H51.data(), sz_H51.size());
    ::LoadStringW(m_hInstResDLL, IDS_H52, sz_H52.data(), sz_H52.size());
    ::LoadStringW(m_hInstResDLL, IDS_H53, sz_H53.data(), sz_H53.size());
    ::LoadStringW(m_hInstResDLL, IDS_H54, sz_H54.data(), sz_H54.size());
    ::LoadStringW(m_hInstResDLL, IDS_H55, sz_H55.data(), sz_H55.size());
    ::LoadStringW(m_hInstResDLL, IDS_H56, sz_H56.data(), sz_H56.size());
    ::LoadStringW(m_hInstResDLL, IDS_H57, sz_H57.data(), sz_H57.size());
    ::LoadStringW(m_hInstResDLL, IDS_H58, sz_H58.data(), sz_H58.size());
    ::LoadStringW(m_hInstResDLL, IDS_H59, sz_H59.data(), sz_H59.size());
    ::LoadStringW(m_hInstResDLL, IDS_H60, sz_H60.data(), sz_H60.size());
    ::LoadStringW(m_hInstResDLL, IDS_H61, sz_H61.data(), sz_H61.size());
    ::LoadStringW(m_hInstResDLL, IDS_H62, sz_H62.data(), sz_H62.size());
    ::LoadStringW(m_hInstResDLL, IDS_H63, sz_H63.data(), sz_H63.size());
    ::LoadStringW(m_hInstResDLL, IDS_H64, sz_H64.data(), sz_H64.size());
    ::LoadStringW(m_hInstResDLL, IDS_H65, sz_H65.data(), sz_H65.size());
    ::LoadStringW(m_hInstResDLL, IDS_H66, sz_H66.data(), sz_H66.size());
    ::LoadStringW(m_hInstResDLL, IDS_H67, sz_H67.data(), sz_H67.size());
    ::LoadStringW(m_hInstResDLL, IDS_H68, sz_H68.data(), sz_H68.size());
    ::LoadStringW(m_hInstResDLL, IDS_H69, sz_H69.data(), sz_H69.size());
    ::LoadStringW(m_hInstResDLL, IDS_H70, sz_H70.data(), sz_H70.size());
    ::LoadStringW(m_hInstResDLL, IDS_H71, sz_H71.data(), sz_H71.size());
    ::LoadStringW(m_hInstResDLL, IDS_H72, sz_H72.data(), sz_H72.size());
    ::LoadStringW(m_hInstResDLL, IDS_H73, sz_H73.data(), sz_H73.size());
    ::LoadStringW(m_hInstResDLL, IDS_I1, sz_I1.data(), sz_I1.size());
    ::LoadStringW(m_hInstResDLL, IDS_I2, sz_I2.data(), sz_I2.size());
    ::LoadStringW(m_hInstResDLL, IDS_I3, sz_I3.data(), sz_I3.size());
    ::LoadStringW(m_hInstResDLL, IDS_I4, sz_I4.data(), sz_I4.size());
    ::LoadStringW(m_hInstResDLL, IDS_J1, sz_J1, _countof(sz_J1));
    ::LoadStringW(m_hInstResDLL, IDS_J2, sz_J2, _countof(sz_J2));
    ::LoadStringW(m_hInstResDLL, IDS_K1, sz_K1.data(), sz_K1.size());
    ::LoadStringW(m_hInstResDLL, IDS_K2, sz_K2.data(), sz_K2.size());
    ::LoadStringW(m_hInstResDLL, IDS_K3, sz_K3.data(), sz_K3.size());
    ::LoadStringW(m_hInstResDLL, IDS_L1, sz_L1.data(), sz_L1.size());
    ::LoadStringW(m_hInstResDLL, IDS_L2, sz_L2.data(), sz_L2.size());
    ::LoadStringW(m_hInstResDLL, IDS_L3, sz_L3.data(), sz_L3.size());
    ::LoadStringW(m_hInstResDLL, IDS_L4, sz_L4.data(), sz_L4.size());
    ::LoadStringW(m_hInstResDLL, IDS_L5, sz_L5.data(), sz_L5.size());
    ::LoadStringW(m_hInstResDLL, IDS_L6, sz_L6.data(), sz_L6.size());
    ::LoadStringW(m_hInstResDLL, IDS_L7, sz_L7.data(), sz_L7.size());
    ::LoadStringW(m_hInstResDLL, IDS_L8, sz_L8.data(), sz_L8.size());
    ::LoadStringW(m_hInstResDLL, IDS_L9, sz_L9.data(), sz_L9.size());
    ::LoadStringW(m_hInstResDLL, IDS_L10, sz_L10.data(), sz_L10.size());
    ::LoadStringW(m_hInstResDLL, IDS_L11, sz_L11.data(), sz_L11.size());
    ::LoadStringW(m_hInstResDLL, IDS_L12, sz_L12.data(), sz_L12.size());
    ::LoadStringW(m_hInstResDLL, IDS_L13, sz_L13.data(), sz_L13.size());
    ::LoadStringW(m_hInstResDLL, IDS_L14, sz_L14.data(), sz_L14.size());
    ::LoadStringW(m_hInstResDLL, IDS_L15, sz_L15.data(), sz_L15.size());
    ::LoadStringW(m_hInstResDLL, IDS_L16, sz_L16.data(), sz_L16.size());
    ::LoadStringW(m_hInstResDLL, IDS_L17, sz_L17.data(), sz_L17.size());
    ::LoadStringW(m_hInstResDLL, IDS_L18, sz_L18.data(), sz_L18.size());
    ::LoadStringW(m_hInstResDLL, IDS_L19, sz_L19.data(), sz_L19.size());
    ::LoadStringW(m_hInstResDLL, IDS_L20, sz_L20.data(), sz_L20.size());
    ::LoadStringW(m_hInstResDLL, IDS_L21, sz_L21.data(), sz_L21.size());
    ::LoadStringW(m_hInstResDLL, IDS_L22, sz_L22.data(), sz_L22.size());
    ::LoadStringW(m_hInstResDLL, IDS_L23, sz_L23.data(), sz_L23.size());
    ::LoadStringW(m_hInstResDLL, IDS_L24, sz_L24.data(), sz_L24.size());
    ::LoadStringW(m_hInstResDLL, IDS_L25, sz_L25.data(), sz_L25.size());
    ::LoadStringW(m_hInstResDLL, IDS_L26, sz_L26.data(), sz_L26.size());
    ::LoadStringW(m_hInstResDLL, IDS_L27, sz_L27.data(), sz_L27.size());
    ::LoadStringW(m_hInstResDLL, IDS_L28, sz_L28.data(), sz_L28.size());
    ::LoadStringW(m_hInstResDLL, IDS_L29, sz_L29.data(), sz_L29.size());
    ::LoadStringW(m_hInstResDLL, IDS_L30, sz_L30.data(), sz_L30.size());
    ::LoadStringW(m_hInstResDLL, IDS_L31, sz_L31.data(), sz_L31.size());
    ::LoadStringW(m_hInstResDLL, IDS_L32, sz_L32.data(), sz_L32.size());
    ::LoadStringW(m_hInstResDLL, IDS_L33, sz_L33.data(), sz_L33.size());
    ::LoadStringW(m_hInstResDLL, IDS_L34, sz_L34.data(), sz_L34.size());
    ::LoadStringW(m_hInstResDLL, IDS_L35, sz_L35.data(), sz_L35.size());
    ::LoadStringW(m_hInstResDLL, IDS_L36, sz_L36.data(), sz_L36.size());
    ::LoadStringW(m_hInstResDLL, IDS_L37, sz_L37.data(), sz_L37.size());
    ::LoadStringW(m_hInstResDLL, IDS_L38, sz_L38.data(), sz_L38.size());
    ::LoadStringW(m_hInstResDLL, IDS_L39, sz_L39.data(), sz_L39.size());
    ::LoadStringW(m_hInstResDLL, IDS_L40, sz_L40.data(), sz_L40.size());
    ::LoadStringW(m_hInstResDLL, IDS_L41, sz_L41.data(), sz_L41.size());
    ::LoadStringW(m_hInstResDLL, IDS_L42, sz_L42.data(), sz_L42.size());
    ::LoadStringW(m_hInstResDLL, IDS_L43, sz_L43.data(), sz_L43.size());
    ::LoadStringW(m_hInstResDLL, IDS_L44, sz_L44.data(), sz_L44.size());
    ::LoadStringW(m_hInstResDLL, IDS_L45, sz_L45.data(), sz_L45.size());
    ::LoadStringW(m_hInstResDLL, IDS_L46, sz_L46.data(), sz_L46.size());
    ::LoadStringW(m_hInstResDLL, IDS_L47, sz_L47.data(), sz_L47.size());
    ::LoadStringW(m_hInstResDLL, IDS_L48, sz_L48.data(), sz_L48.size());
    ::LoadStringW(m_hInstResDLL, IDS_L49, sz_L49.data(), sz_L49.size());
    ::LoadStringW(m_hInstResDLL, IDS_L50, sz_L50.data(), sz_L50.size());
    ::LoadStringW(m_hInstResDLL, IDS_L51, sz_L51.data(), sz_L51.size());
    ::LoadStringW(m_hInstResDLL, IDS_L52, sz_L52.data(), sz_L52.size());
    ::LoadStringW(m_hInstResDLL, IDS_L53, sz_L53.data(), sz_L53.size());
    ::LoadStringW(m_hInstResDLL, IDS_L54, sz_L54.data(), sz_L54.size());
    ::LoadStringW(m_hInstResDLL, IDS_L55, sz_L55.data(), sz_L55.size());
    ::LoadStringW(m_hInstResDLL, IDS_L56, sz_L56.data(), sz_L56.size());
    ::LoadStringW(m_hInstResDLL, IDS_L57, sz_L57.data(), sz_L57.size());
    ::LoadStringW(m_hInstResDLL, IDS_L58, sz_L58.data(), sz_L58.size());
    ::LoadStringW(m_hInstResDLL, IDS_L59, sz_L59.data(), sz_L59.size());
    ::LoadStringW(m_hInstResDLL, IDS_L60, sz_L60.data(), sz_L60.size());
    ::LoadStringW(m_hInstResDLL, IDS_L61, sz_L61.data(), sz_L61.size());
    ::LoadStringW(m_hInstResDLL, IDS_L62, sz_L62.data(), sz_L62.size());
    ::LoadStringW(m_hInstResDLL, IDS_L63, sz_L63.data(), sz_L63.size());
    ::LoadStringW(m_hInstResDLL, IDS_L64, sz_L64.data(), sz_L64.size());
    ::LoadStringW(m_hInstResDLL, IDS_L65, sz_L65.data(), sz_L65.size());
    ::LoadStringW(m_hInstResDLL, IDS_L66, sz_L66.data(), sz_L66.size());
    ::LoadStringW(m_hInstResDLL, IDS_L67, sz_L67.data(), sz_L67.size());
    ::LoadStringW(m_hInstResDLL, IDS_L68, sz_L68.data(), sz_L68.size());
    ::LoadStringW(m_hInstResDLL, IDS_L69, sz_L69.data(), sz_L69.size());
    ::LoadStringW(m_hInstResDLL, IDS_L70, sz_L70.data(), sz_L70.size());
    ::LoadStringW(m_hInstResDLL, IDS_L71, sz_L71.data(), sz_L71.size());
    ::LoadStringW(m_hInstResDLL, IDS_L72, sz_L72.data(), sz_L72.size());
    ::LoadStringW(m_hInstResDLL, IDS_L73, sz_L73.data(), sz_L73.size());
    ::LoadStringW(m_hInstResDLL, IDS_L74, sz_L74.data(), sz_L74.size());
    ::LoadStringW(m_hInstResDLL, IDS_L75, sz_L75.data(), sz_L75.size());
    ::LoadStringW(m_hInstResDLL, IDS_L76, sz_L76.data(), sz_L76.size());
    ::LoadStringW(m_hInstResDLL, IDS_L77, sz_L77.data(), sz_L77.size());
    ::LoadStringW(m_hInstResDLL, IDS_L78, sz_L78.data(), sz_L78.size());
    ::LoadStringW(m_hInstResDLL, IDS_L79, sz_L79.data(), sz_L79.size());
    ::LoadStringW(m_hInstResDLL, IDS_L80, sz_L80.data(), sz_L80.size());
    ::LoadStringW(m_hInstResDLL, IDS_L81, sz_L81.data(), sz_L81.size());
    ::LoadStringW(m_hInstResDLL, IDS_L82, sz_L82.data(), sz_L82.size());
    ::LoadStringW(m_hInstResDLL, IDS_L83, sz_L83.data(), sz_L83.size());
    ::LoadStringW(m_hInstResDLL, IDS_L84, sz_L84.data(), sz_L84.size());
    ::LoadStringW(m_hInstResDLL, IDS_L85, sz_L85.data(), sz_L85.size());
    ::LoadStringW(m_hInstResDLL, IDS_L86, sz_L86.data(), sz_L86.size());
    ::LoadStringW(m_hInstResDLL, IDS_L87, sz_L87.data(), sz_L87.size());
    ::LoadStringW(m_hInstResDLL, IDS_L88, sz_L88.data(), sz_L88.size());
    ::LoadStringW(m_hInstResDLL, IDS_L89, sz_L89.data(), sz_L89.size());
    ::LoadStringW(m_hInstResDLL, IDS_L90, sz_L90.data(), sz_L90.size());
    ::LoadStringW(m_hInstResDLL, IDS_L91, sz_L91.data(), sz_L91.size());
    ::LoadStringW(m_hInstResDLL, IDS_L92, sz_L92.data(), sz_L92.size());
    ::LoadStringW(m_hInstResDLL, IDS_L93, sz_L93.data(), sz_L93.size());
    ::LoadStringW(m_hInstResDLL, IDS_L94, sz_L94.data(), sz_L94.size());
    ::LoadStringW(m_hInstResDLL, IDS_L95, sz_ShowOptions.data(), sizeof(sz_ShowOptions) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L96, sz_HideOptions.data(), sizeof(sz_HideOptions) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L97, sz_Computer.data(), sizeof(sz_Computer) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L98, sz_ID.data(), sizeof(sz_ID) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_L99, sz_Port.data(), sizeof(sz_Port) / sizeof(TCHAR));
    ::LoadStringW(m_hInstResDLL, IDS_M1, sz_M1.data(), sz_M1.size());
    ::LoadStringW(m_hInstResDLL, IDS_M2, sz_M2.data(), sz_M2.size());
    ::LoadStringW(m_hInstResDLL, IDS_M3, sz_M3.data(), sz_M3.size());
    ::LoadStringW(m_hInstResDLL, IDS_M4, sz_M4.data(), sz_M4.size());
    ::LoadStringW(m_hInstResDLL, IDS_M5, sz_M5.data(), sz_M5.size());
    ::LoadStringW(m_hInstResDLL, IDS_N1, sz_N1.data(), sz_N1.size());
    ::LoadStringW(m_hInstResDLL, IDS_N2, sz_N2.data(), sz_N2.size());
    ::LoadStringW(m_hInstResDLL, IDS_N3, sz_N3.data(), sz_N3.size());
    ::LoadStringW(m_hInstResDLL, IDS_N4, sz_N4.data(), sz_N4.size());
    ::LoadStringW(m_hInstResDLL, IDS_N5, sz_N5.data(), sz_N5.size());
    ::LoadStringW(m_hInstResDLL, IDS_N6, sz_N6.data(), sz_N6.size());
    ::LoadStringW(m_hInstResDLL, IDS_N7, sz_N7.data(), sz_N7.size());
    ::LoadStringW(m_hInstResDLL, IDS_N8, sz_N8.data(), sz_N8.size());
    ::LoadStringW(m_hInstResDLL, IDS_N9, sz_N9.data(), sz_N9.size());
    ::LoadStringW(m_hInstResDLL, IDS_N10, sz_N10.data(), sz_N10.size());
    ::LoadStringW(m_hInstResDLL, IDS_N11, sz_N11.data(), sz_N11.size());
    ::LoadStringW(m_hInstResDLL, IDS_N12, sz_N12.data(), sz_N12.size());
    ::LoadStringW(m_hInstResDLL, IDS_N13, sz_N13.data(), sz_N13.size());
    ::LoadStringW(m_hInstResDLL, IDS_N14, sz_N14.data(), sz_N14.size());
}
