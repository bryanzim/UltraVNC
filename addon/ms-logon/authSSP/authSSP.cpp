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


#include "authSSP.h"

#include <array>
#include <cstddef>
#include <cstring>

/*
 *  AuthSSP.cpp: Domainuser could be 'domain\user', just 'user' or
 *  UPN-style 'user@domain'. Should work with Windows NT4 and better.
 *  Windows NT4 does not support UPN-style names.
 */

Fn fn;

BOOL CUPSD2(const char* const domainuser, const char* const password, const PSECURITY_DESCRIPTOR psdSD, const PBOOL isAuthenticated,
            const PDWORD pdwAccessGranted) // returns bitmask with accessrights
{
    std::array<char, MAXLEN> domain;
    const char* user = 0;
    domain[0U] = '\0';
    std::array<TCHAR, MAXLEN> user2;
    std::array<TCHAR, MAXLEN> domain2;
    std::array<TCHAR, MAXLEN> password2;
    [[maybe_unused]] TCHAR* prefix = nullptr;

    user = SplitString(domainuser, '\\', domain.data(), static_cast<int>(domain.size()));

#if defined(_UNICODE) || defined(UNICODE)
    std::size_t pnconv;
    ::mbstowcs_s(&pnconv, user2.data(), user2.size(), user, MAXLEN);
    ::mbstowcs_s(&pnconv, domain2.data(), domain2.size(), domain.data(), domain.size());
    ::mbstowcs_s(&pnconv, password2.data(), password2.size(), password, MAXLEN);
#else
    std::strcpy(user2.data(), user);
    std::strcpy(domain2.data(), domain.data());
    std::strcpy(password2.data(), password);
#endif

    // On NT4, prepend computer- or domainname if username is unqualified.
    if (isNT4() && (::_tcscmp(domain2.data(), _T("")) == 0))
    {
        if (!QualifyName(user2.data(), domain2.data()))
        {
            ::_tcscpy_s(domain2.data(), domain2.size(), _T("\0"));
        }
    }
    return SSPLogonUser(domain2.data(), user2.data(), password2.data(), psdSD, isAuthenticated, pdwAccessGranted);
}


BOOL WINAPI SSPLogonUser(const LPTSTR szDomain, const LPTSTR szUser, const LPTSTR szPassword, const PSECURITY_DESCRIPTOR psdSD,
                         PBOOL isAuthenticated, const PDWORD pdwAccessGranted) // returns bitmask with accessrights
{
    AUTH_SEQ asServer{ 0 };
    AUTH_SEQ asClient{ 0 };
    BOOL fDone = FALSE;
    BOOL fResult = FALSE;
    DWORD cbOut{ 0U };
    DWORD cbIn{ 0U };
    DWORD cbMaxToken{ 0U };
    PVOID pClientBuf = nullptr;
    PVOID pServerBuf = nullptr;
    PSecPkgInfo pSPI = nullptr;
    HMODULE hModule = nullptr;
    SEC_WINNT_AUTH_IDENTITY ai;

    __try
    {

        hModule = LoadSecurityDll();
        if (!hModule)
        {
            __leave;
        }

        // Get max token size
        fn._QuerySecurityPackageInfo((SEC_WCHAR*)L"NTLM", &pSPI);
        cbMaxToken = pSPI->cbMaxToken;

        // Allocate buffers for client and server messages
        pClientBuf = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbMaxToken);
        pServerBuf = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbMaxToken);

        // Initialize auth identity structure
        // Marscha 2004: Seems to work with szDomain = "" or even szDomain = "anyDomain",
        // but I found no MS documentation for this 'feature'.
        ::ZeroMemory(&ai, sizeof(ai));
#if defined(UNICODE) || defined(_UNICODE)
        ai.Domain = (unsigned short*)szDomain;
        ai.DomainLength = ::lstrlen(szDomain);
        ai.User = (unsigned short*)szUser;
        ai.UserLength = ::lstrlen(szUser);
        ai.Password = (unsigned short*)szPassword;
        ai.PasswordLength = ::lstrlen(szPassword);
        ai.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
#else
        ai.Domain = (unsigned char*)szDomain;
        ai.DomainLength = ::lstrlen(szDomain);
        ai.User = (unsigned char*)szUser;
        ai.UserLength = ::lstrlen(szUser);
        ai.Password = (unsigned char*)szPassword;
        ai.PasswordLength = ::lstrlen(szPassword);
        ai.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
#endif

        // Prepare client message (negotiate) .
        cbOut = cbMaxToken;
        if (!GenClientContext(&asClient, &ai, nullptr, 0, pClientBuf, &cbOut, &fDone))
        {
            __leave;
        }

        // Prepare server message (challenge) .
        cbIn = cbOut;
        cbOut = cbMaxToken;
        if (!GenServerContext(&asServer, pClientBuf, cbIn, pServerBuf, &cbOut, &fDone))
        {
            __leave;
        }
        // Most likely failure: AcceptServerContext fails with SEC_E_LOGON_DENIED
        // in the case of bad szUser or szPassword.
        // Unexpected Result: Logon will succeed if you pass in a bad szUser and
        // the guest account is enabled in the specified domain.

        // Prepare client message (authenticate) .
        cbIn = cbOut;
        cbOut = cbMaxToken;
        if (!GenClientContext(&asClient, &ai, pServerBuf, cbIn, pClientBuf, &cbOut, &fDone))
        {
            __leave;
        }

        // Prepare server message (authentication) .
        cbIn = cbOut;
        cbOut = cbMaxToken;
        if (!GenServerContext(&asServer, pClientBuf, cbIn, pServerBuf, &cbOut, &fDone))
        {
            __leave;
        }

        *isAuthenticated = TRUE;

        // Check authorization
        if (IsImpersonationAllowed())
        {
            if (ImpersonateAndCheckAccess(&(asServer.hctxt), psdSD, pdwAccessGranted))
            {
                fResult = TRUE;
            }
        }
        else
        {
            // Todo: Make alternative access check
            if (ImpersonateAndCheckAccess(&(asServer.hctxt), psdSD, pdwAccessGranted))
            {
                fResult = TRUE;
            }
        }
    }
    __finally
    {

        // Clean up resources
        if (pSPI)
        {
            fn._FreeContextBuffer(pSPI);
        }

        if (asClient.fHaveCtxtHandle)
        {
            fn._DeleteSecurityContext(&asClient.hctxt);
        }

        if (asClient.fHaveCredHandle)
        {
            fn._FreeCredentialsHandle(&asClient.hcred);
        }

        if (asServer.fHaveCtxtHandle)
        {
            fn._DeleteSecurityContext(&asServer.hctxt);
        }

        if (asServer.fHaveCredHandle)
        {
            fn._FreeCredentialsHandle(&asServer.hcred);
        }

        if (hModule)
        {
            UnloadSecurityDll(hModule);
        }

        ::HeapFree(::GetProcessHeap(), 0, pClientBuf);
        ::HeapFree(::GetProcessHeap(), 0, pServerBuf);
        ::SecureZeroMemory(&ai, sizeof(ai));
    }

    return fResult;
}

BOOL ImpersonateAndCheckAccess(const PCtxtHandle phContext, const PSECURITY_DESCRIPTOR psdSD, const PDWORD pdwAccessGranted)
{
    HANDLE hToken = nullptr;

    // AccessCheck() variables
    DWORD dwAccessDesired = MAXIMUM_ALLOWED;
    PRIVILEGE_SET PrivilegeSet;
    DWORD dwPrivSetSize = sizeof(PRIVILEGE_SET);
    BOOL fAccessGranted = FALSE;
    GENERIC_MAPPING GenericMapping{ vncGenericRead, vncGenericWrite, vncGenericExecute, vncGenericAll };

    // This only does something if we want to use generic access
    // rights, like GENERIC_ALL, in our call to AccessCheck().
    ::MapGenericMask(&dwAccessDesired, &GenericMapping);

    // AccessCheck() requires an impersonation token.
    if ((fn._ImpersonateSecurityContext(phContext) == SEC_E_OK) &&
        OpenThreadToken(::GetCurrentThread(), TOKEN_QUERY, TRUE, &hToken) &&
        AccessCheck(psdSD, hToken, dwAccessDesired, &GenericMapping, &PrivilegeSet, &dwPrivSetSize, pdwAccessGranted,
                    &fAccessGranted))
    {
        // Restrict access to relevant rights only
        fAccessGranted = ::AreAnyAccessesGranted(*pdwAccessGranted, ViewOnly | Interact);
    }

    // End impersonation
    fn._RevertSecurityContext(phContext);

    // Close handles
    if (hToken)
    {
        ::CloseHandle(hToken);
    }

    return fAccessGranted;
}

// SplitString splits a string 'input' on the first occurence of char 'separator'
// into string 'head' and 'tail' (removing the separator).
// If separator is not found, head = "" and tail = input.
const char* SplitString(const char* const input, const char separator, char* const head, const int sizeHead)
{
    const char* tail;
    int l;

    tail = std::strchr(input, separator);
    if (tail)
    {
        l = static_cast<int>((tail - input));
        // get rid of separator
        tail = tail + 1;
        ::strncpy_s(head, sizeHead, input, l);
        head[l] = '\0';
    }
    else
    {
        tail = input;
        head[0U] = '\0';
    }
    return tail;
}

bool IsImpersonationAllowed()
{
    bool isImpersonationAllowed{ false };
    HANDLE hToken = nullptr;
    DWORD dwReturnLength{ 0U };
    LUID impersonatePrivilege;
    TOKEN_PRIVILEGES* ptp = nullptr;

    if (!::LookupPrivilegeValue(nullptr, _T("SeImpersonatePrivilege"), &impersonatePrivilege) &&
        (::GetLastError() == ERROR_NO_SUCH_PRIVILEGE))
    {
        // Assume that SeImpersonatePrivilege is not implemented with this OS/SP.
        isImpersonationAllowed = true;
    }
    else
    {
        if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &hToken) &&
            !::GetTokenInformation(hToken, TokenPrivileges, nullptr, 0, &dwReturnLength) &&
            (::GetLastError() == ERROR_INSUFFICIENT_BUFFER))
        {
            ptp = (TOKEN_PRIVILEGES*)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, dwReturnLength);
            if (::GetTokenInformation(hToken, TokenPrivileges, ptp, dwReturnLength, &dwReturnLength))
            {
                for (unsigned int i{ 0U }; i < ptp->PrivilegeCount; ++i)
                {
                    // Luid.LowPart/HighPart is ugly. To be improved/changed.
                    if ((ptp->Privileges[i].Luid.LowPart == impersonatePrivilege.LowPart) &&
                        (ptp->Privileges[i].Luid.HighPart == impersonatePrivilege.HighPart) &&
                        (ptp->Privileges[i].Attributes & SE_PRIVILEGE_ENABLED))
                    {
                        isImpersonationAllowed = true;
                    }
                }
            }
        }
    }
    // Close handles
    if (hToken)
    {
        ::CloseHandle(hToken);
    }
    if (ptp)
    {
        ::HeapFree(::GetProcessHeap(), 0, ptp);
    }

    return isImpersonationAllowed;
}

bool QualifyName(const TCHAR* const user, LPTSTR DomName)
{
    PSID pSid = nullptr;
    DWORD cbSid{ 0U };
    DWORD cbDomName = MAXLEN;
    SID_NAME_USE sidUse;
    bool isNameQualified{ false };

    __try
    {
        // Get Sid buffer size
        ::LookupAccountName(nullptr, user, pSid, &cbSid, DomName, &cbDomName, &sidUse);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER)
        {
            __leave;
        }
        if (!(pSid = (PSID)::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbSid)))
        {
            __leave;
        }
        if (cbDomName > MAXLEN)
        {
            __leave;
        }
        // Get DomName
        if (!::LookupAccountName(nullptr, user, pSid, &cbSid, DomName, &cbDomName, &sidUse))
        {
            __leave;
        }
        isNameQualified = true;
    }
    __finally
    {
        ::HeapFree(::GetProcessHeap(), 0, pSid);
    }

    return isNameQualified;
}

bool isNT4()
{
    OSVERSIONINFO VerInfo;

    VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&VerInfo))
    { // If this fails, something has gone wrong
        return false;
    }

    if ((VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) && (VerInfo.dwMajorVersion == 4) && (VerInfo.dwMinorVersion == 0))
    {
        return true;
    }
    else
    {
        return false;
    }
}