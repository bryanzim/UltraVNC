// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.


#include "stdhdrs.h"

#include "credentials.h"

#include "common/ScopeGuard.h"
#include "SettingsManager.h"
#include "vncOSVersion.h"

#include <lmcons.h>
#include <wtsapi32.h>

#include <array>
#include <cstring>

DesktopUsersToken* DesktopUsersToken::instance = nullptr;

DWORD Credentials::GetCurrentUserToken(HANDLE& process, HANDLE& Token, const bool RunningFromExternalService)
{
    if (!RunningFromExternalService)
    {
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS, &Token))
        {
            return 0;
        }
        return 2;
    }

    HWND tray = ::FindWindow(("Shell_TrayWnd"), 0);
    if (!tray)
    {
        return 0;
    }
    DWORD processId{ 0U };
    ::GetWindowThreadProcessId(tray, &processId);
    if (!processId)
    {
        return 0;
    }
    process = ::OpenProcess(MAXIMUM_ALLOWED, FALSE, processId);
    if (!process)
    {
        return 0;
    }
    ::OpenProcessToken(process, MAXIMUM_ALLOWED, &Token);
    return 2;
}

bool Credentials::RunningAsAdministrator(const bool RunningFromExternalService)
{
    BOOL fAdmin = FALSE;
    TOKEN_GROUPS* ptg = nullptr;
    DWORD cbTokenGroups{ 0U };
    DWORD dwGroup{ 0U };
    PSID psidAdmin = nullptr;
    SetLastError(0);
    HANDLE process = nullptr;
    HANDLE Token = nullptr;

    if (!GetCurrentUserToken(process, Token, RunningFromExternalService) == 1)
    {
        return false;
    }

    ON_BLOCK_EXIT(CloseHandle, process);
    ON_BLOCK_EXIT(CloseHandle, Token);

    SID_IDENTIFIER_AUTHORITY SystemSidAuthority = SECURITY_NT_AUTHORITY;

    // Get size of the group information
    if (!::GetTokenInformation(Token, TokenGroups, nullptr, 0, &cbTokenGroups))
    {
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            return false;
        }
    }

    // Allocate memory for group information
    ptg = (TOKEN_GROUPS*)_malloca(cbTokenGroups);
    if (!ptg)
    {
        return false;
    }

    // Retrieve group information
    if (!::GetTokenInformation(Token, TokenGroups, ptg, cbTokenGroups, &cbTokenGroups))
    {
        return false;
    }

    // Create SID for Administrators group
    if (!::AllocateAndInitializeSid(&SystemSidAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0,
                                  &psidAdmin))
    {
        return false;
    }

    // Check group membership
    for (dwGroup = 0; dwGroup < ptg->GroupCount; ++dwGroup)
    {
        if (::EqualSid(ptg->Groups[dwGroup].Sid, psidAdmin))
        {
            fAdmin = TRUE;
            break;
        }
    }

    /// Cleanup SID
    if (psidAdmin)
    {
        ::FreeSid(psidAdmin);
    }

    return fAdmin;
}

DesktopUsersToken* DesktopUsersToken::getInstance()
{
    if (instance == nullptr)
    {
        instance = new DesktopUsersToken();
    }
    return instance;
}

DesktopUsersToken::DesktopUsersToken()
{
    hProcess = nullptr;
    hPToken = nullptr;
}

DesktopUsersToken::~DesktopUsersToken()
{
    if (hProcess)
    {
        ::CloseHandle(hProcess);
    }
    if (hPToken)
    {
        ::CloseHandle(hPToken);
    }
}

HANDLE DesktopUsersToken::getDesktopUsersToken()
{
    DWORD explorerLogonPid = processHelper::GetExplorerLogonPid();

    if ((explorerLogonPid != 0) && (dwExplorerLogonPid != explorerLogonPid))
    {
        vnclog.Print(LL_INTWARN, VNCLOG("GetExplorerLogonPid %i\n"), explorerLogonPid);
        if (hProcess)
        {
            ::CloseHandle(hProcess);
            hProcess = nullptr;
        }
        if (hPToken)
        {
            ::CloseHandle(hPToken);
            hPToken = nullptr;
        }
        hProcess = ::OpenProcess(MAXIMUM_ALLOWED, FALSE, explorerLogonPid);
        if (hProcess == nullptr)
        {
            vnclog.Print(LL_LOGSCREEN, "UsersToken Failed");
            vnclog.Print(LL_INTWARN, VNCLOG("DesktopUsersToken failed OpenProcess error %i\n"), ::GetLastError());
            dwExplorerLogonPid = 0;
            return nullptr;
        }
        vnclog.Print(LL_LOGSCREEN, "UsersToken found");
        if (!::OpenProcessToken(hProcess,
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY | TOKEN_DUPLICATE | TOKEN_ASSIGN_PRIMARY |
                                  TOKEN_ADJUST_SESSIONID | TOKEN_READ | TOKEN_WRITE,
                              &hPToken))
        {
            vnclog.Print(LL_INTWARN, VNCLOG("OpenProcessToken failed  %i\n"), ::GetLastError());
            vnclog.Print(LL_LOGSCREEN, "OpenProcessToken Failed");
            dwExplorerLogonPid = 0;
            return nullptr;
        }
        vnclog.Print(LL_LOGSCREEN, "OpenProcessToken OK");
        dwExplorerLogonPid = explorerLogonPid;
    }
    else if (explorerLogonPid == 0)
    {
        vnclog.Print(LL_LOGSCREEN, "explorer shell NOT found, trying WTSQueryUserToken");
        // No explorer/shell running (kiosk mode) - fall back to active console session token
        DWORD dwSessionId = ::WTSGetActiveConsoleSessionId();
        HANDLE hWtsToken = nullptr;
        if (::WTSQueryUserToken(dwSessionId, &hWtsToken))
        {
            if (hPToken)
            {
                ::CloseHandle(hPToken);
            }
            hPToken = hWtsToken;
            dwExplorerLogonPid = 0;
            vnclog.Print(LL_LOGSCREEN, "WTSQueryUserToken OK (kiosk/no-shell mode)");
        }
        else
        {
            vnclog.Print(LL_INTWARN, VNCLOG("WTSQueryUserToken failed error %i\n"), ::GetLastError());
        }
    }

    return hPToken;
}

bool DesktopUsersToken::GetConsoleUser(char* const buffer, UINT size)
{
    HANDLE hPToken = DesktopUsersToken::getInstance()->getDesktopUsersToken();
    if (hPToken == nullptr)
    {
        ::strcpy_s(buffer, UNLEN + 1, "");
        return 0;
    }

    if (hPToken == save_hPtoken)
    {
        ::strcpy_s(buffer, UNLEN + 1, username);
        return std::strlen(username) != 0;
    }

    save_hPtoken = hPToken;
    std::array<char, 16384U> aa{};
    // token user
    TOKEN_USER* ptu;
    DWORD needed;
    ptu = (TOKEN_USER*)aa.data();
    if (::GetTokenInformation(hPToken, TokenUser, ptu, 16384, &needed))
    {
        std::array<char, 64U> DomainName;
        std::memset(DomainName.data(), 0, sizeof(DomainName));
        DWORD DomainSize;
        DomainSize = sizeof(DomainName) - 1;
        SID_NAME_USE SidType;
        DWORD dwsize = size;
        ::LookupAccountSid(nullptr, ptu->User.Sid, buffer, &dwsize, DomainName.data(), &DomainSize, &SidType);
        ::strcpy_s(username, UNLEN + 1, buffer);
        return 1;
    }
    ::strcpy_s(buffer, UNLEN + 1, "");
    ::strcpy_s(username, UNLEN + 1, "");
    return 0;
}