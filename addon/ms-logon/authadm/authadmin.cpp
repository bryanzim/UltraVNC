// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// /macine-vnc Greg Wood (wood@agressiv.com)


#include "authadmin.h"

#include <lm.h>

#include <array>
#include <cstddef>
#include <cstdio>

typedef struct _WKSTA_INFO_100_NT
{
    DWORD wki100_platform_id;
    wchar_t* wki100_computername;
    wchar_t* wki100_langroup;
    DWORD wki100_ver_major;
    DWORD wki100_ver_minor;
} WKSTA_INFO_100_NT;


BOOL APIENTRY DllMain(const HANDLE hModule, const DWORD ul_reason_for_call, const LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


bool IsAdmin()
{
    SC_HANDLE hSC;
    hSC = ::OpenSCManager(nullptr, nullptr, GENERIC_READ | GENERIC_WRITE | GENERIC_EXECUTE);
    if (hSC == nullptr)
    {
        return FALSE;
    }
    ::CloseServiceHandle(hSC);
    return TRUE;
}

AUTHADMIN_API
BOOL CUGP(char* const userin, char* const password, char* const machine, char* const groupin, const int locdom)
{
    DWORD dwLogonType;
    DWORD dwLogonProvider;
    HANDLE hToken;
    bool returnvalue{ false };
    dwLogonType = LOGON32_LOGON_INTERACTIVE;
    dwLogonProvider = LOGON32_PROVIDER_DEFAULT;

    byte* buf = 0;
    byte* buf2 = 0;
    std::array<char, MAXLEN * sizeof(wchar_t)> domain;
    DWORD rcdomain = ::NetGetDCName(0, 0, &buf);
    NetApiBufferFree(buf);
    std::printf("Logonuser: % s %s \n", userin, ".");
    if (::LogonUser(userin, ".", password, dwLogonType, dwLogonProvider, &hToken))
        if (::ImpersonateLoggedOnUser(hToken))
        {
            returnvalue = IsAdmin();
            ::RevertToSelf();
            ::CloseHandle(hToken);
        }
    if (returnvalue == true)
    {
        return returnvalue;
    }
    if (!rcdomain)
    {
        DWORD result = ::NetWkstaGetInfo(0, 100, &buf2);
        if (!result)
        {
            std::size_t pnconv;
            ::wcstombs_s(&pnconv, domain.data(), MAXLEN, ((WKSTA_INFO_100_NT*)buf2)->wki100_langroup, MAXLEN);
            NetApiBufferFree(buf2);
            std::printf("Logonuser: % s %s \n", userin, domain.data());
            if (::LogonUser(userin, domain.data(), password, dwLogonType, dwLogonProvider, &hToken))
                if (::ImpersonateLoggedOnUser(hToken))
                {
                    returnvalue = IsAdmin();
                    ::RevertToSelf();
                    ::CloseHandle(hToken);
                }
        }
    }

    return returnvalue;
}