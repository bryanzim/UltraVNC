// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2004 Martin Scharpf. All Rights Reserved.

#pragma warning(disable : 4996)

// /macine-vnc Greg Wood (wood@agressiv.com)


//#include "../../winvnc/stdhdrs.h"
#include "vncSSP.h"

#include <objbase.h> // for CoInitialize/CoUninitialize ???

#include <array>
#include <cstddef>
#include <cstring>
#include <time.h>
// From vncAccessControl.h

#define ViewOnly 0x0001
#define Interact 0x0002

#include "../../../winvnc/winvnc/localization.h" // Act : add localization on messages

#include <cstdio>
#include <ctime>

CheckUserPasswordSDFn CheckUserPasswordSD = 0;

const TCHAR REGISTRY_KEY[] = _T("Software\\UltraVNC");

AUTHSSP_API
int CUPSD(const char* const userin, const char* const password, const char* const machine)
{
    DWORD dwAccessGranted{ 0U };
    BOOL isAccessOK = FALSE;
    const BOOL NT4OS = FALSE;
    const BOOL W2KOS = FALSE;
    BOOL isAuthenticated = FALSE;
    bool isViewOnly{ false };
    bool isInteract{ false };
    std::array<TCHAR, MAXSTRING> machine2;
    std::array<TCHAR, MAXSTRING> user2;
#if defined(UNICODE) || defined(_UNICODE)
    std::size_t pnconv;
    ::mbstowcs_s(&pnconv, machine2.data(), machine2.size(), machine, MAXSTRING);
    ::mbstowcs_s(&pnconv, user2.data(), user2.size(), userin, MAXSTRING);
#else
    std::strcpy(machine2.data(), machine);
    std::strcpy(user2.data(), userin);
#endif

    OSVERSIONINFO VerInfo;
    VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&VerInfo))
    { // If this fails, something has gone wrong
        return FALSE;
    }

    if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    { // Windows NT 3.51 or better
        vncAccessControl vncAC;
        isAccessOK = CUPSD2(userin, password, vncAC.GetSD(), &isAuthenticated, &dwAccessGranted);
        // This logging should be moved to LOGLOGONUSER etc.
        FILE* file = std::fopen("WinVNC-authSSP.log", "a");
        if (file)
        {
            time_t current;
            std::time(&current);
            std::array<char, 50U> timestr;
            ::ctime_s(timestr.data(), 50, &current);
            timestr[49U] = '\0'; // remove newline
            std::fprintf(file, "%s - CUPSD2: Access is %u, user %s is %sauthenticated, access granted is 0x%x\n", timestr.data(),
                    isAccessOK, userin, isAuthenticated ? "" : "not ", dwAccessGranted);
            std::fclose(file);
        }
    }
    else
    { // message text to be moved to localization.h
        ::MessageBox(nullptr, _T("New MS-Logon currently not supported on Win9x"), _T("Warning"), MB_OK);
        return FALSE;
    }

    if (isAccessOK)
    {
        if (dwAccessGranted & ViewOnly)
        {
            isViewOnly = true;
        }
        if (dwAccessGranted & Interact)
        {
            isInteract = true;
        }
    }

    //LookupAccountName(NULL, user2, Sid, cbSid, DomainName, cbDomainName, peUse);

    if (isInteract)
    {
        LOG(0x00640001L, _T("MS-Logon authentication accepted from %s using %s account (Interactive)\n"), machine2.data(),
            user2.data());
    }
    else if (isViewOnly)
    {
        LOG(0x00640001L, _T("MS-Logon authentication accepted from %s using %s account (ViewOnly)\n"), machine2.data(),
            user2.data());
        isAccessOK = 2;
    }
    else
    {
        LOG(0x00640002L, _T("MS-Logon authentication refused from %s using %s account (not %s)\n"), machine2.data(), user2.data(),
            isAuthenticated ? _T("authorized") : _T("authenticated"));
    }
    return isAccessOK;
}

AUTHSSP_API
int CUPSDV2(const char* const userin, const char* const password, const char* const machine, TCHAR* const szMslogonLog)
{
    DWORD dwAccessGranted{ 0U };
    BOOL isAccessOK = FALSE;
    const BOOL NT4OS = FALSE;
    const BOOL W2KOS = FALSE;
    BOOL isAuthenticated = FALSE;
    bool isViewOnly{ false };
    bool isInteract{ false };
    std::array<TCHAR, MAXSTRING> machine2;
    std::array<TCHAR, MAXSTRING> user2;
#if defined(UNICODE) || defined(_UNICODE)
    std::size_t pnconv;
    ::mbstowcs_s(&pnconv, machine2.data(), machine2.size(), machine, MAXSTRING);
    ::mbstowcs_s(&pnconv, user2.data(), user2.size(), userin, MAXSTRING);
#else
    std::strcpy(machine2.data(), machine);
    std::strcpy(user2.data(), userin);
#endif

    OSVERSIONINFO VerInfo;
    VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&VerInfo))
    { // If this fails, something has gone wrong
        return FALSE;
    }

    if (VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT)
    { // Windows NT 3.51 or better
        vncAccessControl vncAC;
        isAccessOK = CUPSD2(userin, password, vncAC.GetSD(), &isAuthenticated, &dwAccessGranted);
        // This logging should be moved to LOGLOGONUSER etc.
        FILE* file = std::fopen("WinVNC-authSSP.log", "a");
        if (file)
        {
            time_t current;
            std::time(&current);
            std::array<char, 50U> timestr;
            ::ctime_s(timestr.data(), 50, &current);
            timestr[49U] = '\0'; // remove newline
            std::fprintf(file, "%s - CUPSD2: Access is %u, user %s is %sauthenticated, access granted is 0x%x\n", timestr.data(),
                    isAccessOK, userin, isAuthenticated ? "" : "not ", dwAccessGranted);
            std::fclose(file);
        }
    }
    else
    { // message text to be moved to localization.h
        ::MessageBox(nullptr, _T("New MS-Logon currently not supported on Win9x"), _T("Warning"), MB_OK);
        return FALSE;
    }

    if (isAccessOK)
    {
        if (dwAccessGranted & ViewOnly)
        {
            isViewOnly = true;
        }
        if (dwAccessGranted & Interact)
        {
            isInteract = true;
        }
    }

    //LookupAccountName(NULL, user2, Sid, cbSid, DomainName, cbDomainName, peUse);

    if (isInteract)
    {
        LOGV2(szMslogonLog, 0x00640001L, _T("MS-Logon authentication accepted from %s using %s account (Interactive)\n"),
              machine2.data(), user2.data());
    }
    else if (isViewOnly)
    {
        LOGV2(szMslogonLog, 0x00640001L, _T("MS-Logon authentication accepted from %s using %s account (ViewOnly)\n"),
              machine2.data(), user2.data());
        isAccessOK = 2;
    }
    else
    {
        LOGV2(szMslogonLog, 0x00640002L, _T("MS-Logon authentication refused from %s using %s account (not %s)\n"), machine2.data(),
              user2.data(), isAuthenticated ? _T("authorized") : _T("authenticated"));
    }
    return isAccessOK;
}


TCHAR* AddToModuleDir(TCHAR* filename, const int length)
{
    TCHAR* szCurrentDir = new TCHAR[length];
    if (::GetModuleFileName(nullptr, szCurrentDir, length))
    {
        TCHAR* p = ::_tcsrchr(szCurrentDir, '\\');
        *p = '\0';
        ::_tcscat_s(szCurrentDir, length, _T("\\"));
        ::_tcscat_s(szCurrentDir, length, filename);
    }
    filename = szCurrentDir;
    return filename;
}