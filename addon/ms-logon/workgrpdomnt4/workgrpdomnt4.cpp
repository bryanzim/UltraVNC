// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//
#pragma warning(disable : 4996)


// /macine-vnc Greg Wood (wood@agressiv.com)


#include "workgrpdomnt4.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwchar>
/////////////////////////

BOOL APIENTRY DllMain([[maybe_unused]] const HANDLE hModule, const DWORD ul_reason_for_call, [[maybe_unused]] const LPVOID lpReserved)
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


///////////////////////////////////////////////////////////////////////////////


void UnloadSecurityDll(const HMODULE hModule)
{

    if (hModule)
    {
        ::FreeLibrary(hModule);
    }

    _AcceptSecurityContext = nullptr;
    _AcquireCredentialsHandle = nullptr;
    _CompleteAuthToken = nullptr;
    _DeleteSecurityContext = nullptr;
    _FreeContextBuffer = nullptr;
    _FreeCredentialsHandle = nullptr;
    _InitializeSecurityContext = nullptr;
    _QuerySecurityPackageInfo = nullptr;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////


HMODULE LoadSecurityDll()
{

    HMODULE hModule;
    BOOL fAllFunctionsLoaded = FALSE;
    std::array<TCHAR, MAX_PATH> lpszDLL;
    OSVERSIONINFO VerInfo;

    _AcceptSecurityContext = nullptr;
    _AcquireCredentialsHandle = nullptr;
    _CompleteAuthToken = nullptr;
    _DeleteSecurityContext = nullptr;
    _FreeContextBuffer = nullptr;
    _FreeCredentialsHandle = nullptr;
    _InitializeSecurityContext = nullptr;
    _QuerySecurityPackageInfo = nullptr;

    //
    //  Find out which security DLL to use, depending on
    //  whether we are on Windows NT or Windows 95 or Windows 2000 or Windows XP or .NET Server
    //  We have to use security.dll on Windows NT4.
    //  All other operating systems, we have to use Secur32.dll
    //
    VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&VerInfo)) // If this fails, something has gone wrong
    {
        return FALSE;
    }

    if ((VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) && (VerInfo.dwMajorVersion == 4) && (VerInfo.dwMinorVersion == 0))
    {
        ::lstrcpy(lpszDLL.data(), _T("security.dll"));
    }
    else
    {
        ::lstrcpy(lpszDLL.data(), _T("secur32.dll"));
    }


    hModule = ::LoadLibrary(lpszDLL.data());
    if (!hModule)
    {
        return nullptr;
    }

    __try
    {

        _AcceptSecurityContext = (ACCEPT_SECURITY_CONTEXT_FN)::GetProcAddress(hModule, "AcceptSecurityContext");
        if (!_AcceptSecurityContext)
        {
            __leave;
        }

#ifdef UNICODE
        _AcquireCredentialsHandle = (ACQUIRE_CREDENTIALS_HANDLE_FN)::GetProcAddress(hModule, "AcquireCredentialsHandleW");
#else
        _AcquireCredentialsHandle = (ACQUIRE_CREDENTIALS_HANDLE_FN)::GetProcAddress(hModule, "AcquireCredentialsHandleA");
#endif
        if (!_AcquireCredentialsHandle)
        {
            __leave;
        }

        // CompleteAuthToken is not present on Windows 9x Secur32.dll
        // Do not check for the availablity of the function if it is NULL;
        _CompleteAuthToken = (COMPLETE_AUTH_TOKEN_FN)::GetProcAddress(hModule, "CompleteAuthToken");

        _DeleteSecurityContext = (DELETE_SECURITY_CONTEXT_FN)::GetProcAddress(hModule, "DeleteSecurityContext");
        if (!_DeleteSecurityContext)
        {
            __leave;
        }

        _FreeContextBuffer = (FREE_CONTEXT_BUFFER_FN)::GetProcAddress(hModule, "FreeContextBuffer");
        if (!_FreeContextBuffer)
        {
            __leave;
        }

        _FreeCredentialsHandle = (FREE_CREDENTIALS_HANDLE_FN)::GetProcAddress(hModule, "FreeCredentialsHandle");
        if (!_FreeCredentialsHandle)
        {
            __leave;
        }

#ifdef UNICODE
        _InitializeSecurityContext = (INITIALIZE_SECURITY_CONTEXT_FN)::GetProcAddress(hModule, "InitializeSecurityContextW");
#else
        _InitializeSecurityContext = (INITIALIZE_SECURITY_CONTEXT_FN)::GetProcAddress(hModule, "InitializeSecurityContextA");
#endif
        if (!_InitializeSecurityContext)
        {
            __leave;
        }

#ifdef UNICODE
        _QuerySecurityPackageInfo = (QUERY_SECURITY_PACKAGE_INFO_FN)::GetProcAddress(hModule, "QuerySecurityPackageInfoW");
#else
        _QuerySecurityPackageInfo = (QUERY_SECURITY_PACKAGE_INFO_FN)::GetProcAddress(hModule, "QuerySecurityPackageInfoA");
#endif
        if (!_QuerySecurityPackageInfo)
        {
            __leave;
        }

        fAllFunctionsLoaded = TRUE;
    }
    __finally
    {

        if (!fAllFunctionsLoaded)
        {
            UnloadSecurityDll(hModule);
            hModule = nullptr;
        }
    }

    return hModule;
}


///////////////////////////////////////////////////////////////////////////////


BOOL GenClientContext(PAUTH_SEQ pAS, const PSEC_WINNT_AUTH_IDENTITY pAuthIdentity, const PVOID pIn, const DWORD cbIn,
                      const PVOID pOut, PDWORD pcbOut, PBOOL pfDone)
{

    /*++

     Routine Description:

       Optionally takes an input buffer coming from the server and returns
       a buffer of information to send back to the server. Also returns
       an indication of whether or not the context is complete.

     Return Value:

       Returns TRUE if successful; otherwise FALSE.

    --*/

    SECURITY_STATUS ss;
    TimeStamp tsExpiry;
    SecBufferDesc sbdOut;
    SecBuffer sbOut;
    SecBufferDesc sbdIn;
    SecBuffer sbIn;
    ULONG fContextAttr;

    if (!pAS->fInitialized)
    {

        ss = _AcquireCredentialsHandle(nullptr, _T("NTLM"), SECPKG_CRED_OUTBOUND, nullptr, pAuthIdentity, nullptr, nullptr,
                                       &pAS->hcred, &tsExpiry);
        if (ss < 0)
        {
            std::fprintf(stderr, "AcquireCredentialsHandle failed with %08X\n", ss);
            return FALSE;
        }

        pAS->fHaveCredHandle = TRUE;
    }

    // Prepare output buffer
    sbdOut.ulVersion = 0;
    sbdOut.cBuffers = 1;
    sbdOut.pBuffers = &sbOut;

    sbOut.cbBuffer = *pcbOut;
    sbOut.BufferType = SECBUFFER_TOKEN;
    sbOut.pvBuffer = pOut;

    // Prepare input buffer
    if (pAS->fInitialized)
    {
        sbdIn.ulVersion = 0;
        sbdIn.cBuffers = 1;
        sbdIn.pBuffers = &sbIn;

        sbIn.cbBuffer = cbIn;
        sbIn.BufferType = SECBUFFER_TOKEN;
        sbIn.pvBuffer = pIn;
    }

    ss = _InitializeSecurityContext(&pAS->hcred, pAS->fInitialized ? &pAS->hctxt : nullptr, nullptr, 0, 0, SECURITY_NATIVE_DREP,
                                    pAS->fInitialized ? &sbdIn : nullptr, 0, &pAS->hctxt, &sbdOut, &fContextAttr, &tsExpiry);
    if (ss < 0)
    {
        // <winerror.h>
        std::fprintf(stderr, "InitializeSecurityContext failed with %08X\n", ss);
        return FALSE;
    }

    pAS->fHaveCtxtHandle = TRUE;

    // If necessary, complete token
    if ((ss == SEC_I_COMPLETE_NEEDED) || (ss == SEC_I_COMPLETE_AND_CONTINUE))
    {

        if (_CompleteAuthToken)
        {
            ss = _CompleteAuthToken(&pAS->hctxt, &sbdOut);
            if (ss < 0)
            {
                std::fprintf(stderr, "CompleteAuthToken failed with %08X\n", ss);
                return FALSE;
            }
        }
        else
        {
            std::fprintf(stderr, "CompleteAuthToken not supported.\n");
            return FALSE;
        }
    }

    *pcbOut = sbOut.cbBuffer;

    if (!pAS->fInitialized)
    {
        pAS->fInitialized = TRUE;
    }

    *pfDone = !((ss == SEC_I_CONTINUE_NEEDED) || (ss == SEC_I_COMPLETE_AND_CONTINUE));

    return TRUE;
}


///////////////////////////////////////////////////////////////////////////////


BOOL GenServerContext(PAUTH_SEQ pAS, const PVOID pIn, const DWORD cbIn, const PVOID pOut, PDWORD pcbOut, PBOOL pfDone)
{

    /*++

     Routine Description:

        Takes an input buffer coming from the client and returns a buffer
        to be sent to the client. Also returns an indication of whether or
        not the context is complete.

     Return Value:

        Returns TRUE if successful; otherwise FALSE.

    --*/

    SECURITY_STATUS ss;
    TimeStamp tsExpiry;
    SecBufferDesc sbdOut;
    SecBuffer sbOut;
    SecBufferDesc sbdIn;
    SecBuffer sbIn;
    ULONG fContextAttr;

    if (!pAS->fInitialized)
    {

        ss = _AcquireCredentialsHandle(nullptr, _T("NTLM"), SECPKG_CRED_INBOUND, nullptr, nullptr, nullptr, nullptr, &pAS->hcred,
                                       &tsExpiry);
        if (ss < 0)
        {
            std::fprintf(stderr, "AcquireCredentialsHandle failed with %08X\n", ss);
            return FALSE;
        }

        pAS->fHaveCredHandle = TRUE;
    }

    // Prepare output buffer
    sbdOut.ulVersion = 0;
    sbdOut.cBuffers = 1;
    sbdOut.pBuffers = &sbOut;

    sbOut.cbBuffer = *pcbOut;
    sbOut.BufferType = SECBUFFER_TOKEN;
    sbOut.pvBuffer = pOut;

    // Prepare input buffer
    sbdIn.ulVersion = 0;
    sbdIn.cBuffers = 1;
    sbdIn.pBuffers = &sbIn;

    sbIn.cbBuffer = cbIn;
    sbIn.BufferType = SECBUFFER_TOKEN;
    sbIn.pvBuffer = pIn;

    ss = _AcceptSecurityContext(&pAS->hcred, pAS->fInitialized ? &pAS->hctxt : nullptr, &sbdIn, 0, SECURITY_NATIVE_DREP,
                                &pAS->hctxt, &sbdOut, &fContextAttr, &tsExpiry);
    if (ss < 0)
    {
        std::fprintf(stderr, "AcceptSecurityContext failed with %08X\n", ss);
        return FALSE;
    }

    pAS->fHaveCtxtHandle = TRUE;

    // If necessary, complete token
    if ((ss == SEC_I_COMPLETE_NEEDED) || (ss == SEC_I_COMPLETE_AND_CONTINUE))
    {

        if (_CompleteAuthToken)
        {
            ss = _CompleteAuthToken(&pAS->hctxt, &sbdOut);
            if (ss < 0)
            {
                std::fprintf(stderr, "CompleteAuthToken failed with %08X\n", ss);
                return FALSE;
            }
        }
        else
        {
            std::fprintf(stderr, "CompleteAuthToken not supported.\n");
            return FALSE;
        }
    }

    *pcbOut = sbOut.cbBuffer;

    if (!pAS->fInitialized)
    {
        pAS->fInitialized = TRUE;
    }

    *pfDone = !(ss = SEC_I_CONTINUE_NEEDED || (ss == SEC_I_COMPLETE_AND_CONTINUE));

    return TRUE;
}

///////////////////////////////////////////////////////////////////////////////

BOOL WINAPI SSPLogonUser(const LPTSTR szDomain, const LPTSTR szUser, const LPTSTR szPassword)
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
        _QuerySecurityPackageInfo(_T("NTLM"), &pSPI);
        cbMaxToken = pSPI->cbMaxToken;
        _FreeContextBuffer(pSPI);

        // Allocate buffers for client and server messages
        pClientBuf = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbMaxToken);
        pServerBuf = ::HeapAlloc(::GetProcessHeap(), HEAP_ZERO_MEMORY, cbMaxToken);

        // Initialize auth identity structure
        ::ZeroMemory(&ai, sizeof(ai));
#if defined(UNICODE) || defined(_UNICODE)
        ai.Domain = szDomain;
        ai.DomainLength = ::lstrlen(szDomain);
        ai.User = szUser;
        ai.UserLength = ::lstrlen(szUser);
        ai.Password = szPassword;
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

        fResult = TRUE;
    }
    __finally
    {

        // Clean up resources
        if (asClient.fHaveCtxtHandle)
        {
            _DeleteSecurityContext(&asClient.hctxt);
        }

        if (asClient.fHaveCredHandle)
        {
            _FreeCredentialsHandle(&asClient.hcred);
        }

        if (asServer.fHaveCtxtHandle)
        {
            _DeleteSecurityContext(&asServer.hctxt);
        }

        if (asServer.fHaveCredHandle)
        {
            _FreeCredentialsHandle(&asServer.hcred);
        }

        if (hModule)
        {
            UnloadSecurityDll(hModule);
        }

        ::HeapFree(::GetProcessHeap(), 0, pClientBuf);
        ::HeapFree(::GetProcessHeap(), 0, pServerBuf);
    }

    return fResult;
}

///////////////////////////////////////////////////////////////////////////////
WORKGRPDOMNT4_API
BOOL CUGP(char* const userin, char* const password, [[maybe_unused]] char* const machine, char* const groupin, const int locdom)
{
    //	FILE *file;
    bool isNT{ true };
    bool isXP{ false };
    bool access_vnc = FALSE;
    bool laccess_vnc = FALSE;
    [[maybe_unused]] const bool localdatabase{ false };
    ad_access = false;
    OSVERSIONINFO ovi{ sizeof ovi };
    // determine OS and load appropriate library

    GetVersionEx(&ovi);
    if (ovi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
    {
        isNT = false;
    }
    if ((ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) && (ovi.dwMajorVersion == 5) && (ovi.dwMinorVersion >= 1))
    {
        isXP = true;
    }

    HINSTANCE hNet = 0, hLoc = 0;

    if (isNT)
    {
        hNet = ::LoadLibrary("netapi32.dll");
    }
    else
    {
        hLoc = ::LoadLibrary("rlocal32.dll");
        hNet = ::LoadLibrary("radmin32.dll");
    }

    if (hNet == 0)
    {
        std::printf("LoadLibrary( %s ) failed, gle = %lu\n", isNT ? "netapi32.dll" : "radmin32.dll", ::GetLastError());
        return false;
    }

    // locate needed functions
    NetApiBufferFree_t NetApiBufferFree = 0;
    NetGetDCNameNT_t NetGetDCNameNT = 0;
    NetGetDCName95_t NetGetDCName95 = 0;
    NetUserGetGroupsNT_t NetUserGetGroupsNT = 0;
    NetUserGetGroupsNT_t2 NetUserGetGroupsNT2 = 0;
    NetUserGetGroups95_t NetUserGetGroups95 = 0;
    NetWkstaGetInfoNT_t NetWkstaGetInfoNT = 0;
    NetWkstaGetInfo95_t NetWkstaGetInfo95 = 0;

    NetApiBufferFree = (NetApiBufferFree_t)::GetProcAddress(hNet, "NetApiBufferFree");
    if (NetApiBufferFree == 0)
    {
        std::printf("Oops! GetProcAddress() failed\n");
        return false;
    }

    if (isNT)
    {
        NetGetDCNameNT = (NetGetDCNameNT_t)::GetProcAddress(hNet, "NetGetDCName");
        NetUserGetGroupsNT = (NetUserGetGroupsNT_t)::GetProcAddress(hNet, "NetUserGetLocalGroups");
        NetUserGetGroupsNT2 = (NetUserGetGroupsNT_t2)::GetProcAddress(hNet, "NetUserGetGroups");
        NetWkstaGetInfoNT = (NetWkstaGetInfoNT_t)::GetProcAddress(hNet, "NetWkstaGetInfo");
        if ((NetGetDCNameNT == 0) || (NetUserGetGroupsNT == 0) || (NetWkstaGetInfoNT == 0) || (NetUserGetGroupsNT2 == 0))
        {
            std::printf("Oops! Some functions not found...\n");
            return false;
        }
    }
    else
    {
        NetGetDCName95 = (NetGetDCName95_t)::GetProcAddress(hLoc, "LocalNetGetDCName");
        NetUserGetGroups95 = (NetUserGetGroups95_t)::GetProcAddress(hNet, "NetUserGetGroupsA");
        NetWkstaGetInfo95 = (NetWkstaGetInfo95_t)::GetProcAddress(hNet, "NetWkstaGetInfoA");

        if ((NetGetDCName95 == 0) || (NetUserGetGroups95 == 0) || (NetWkstaGetInfo95 == 0))
        {
            std::printf("Oops! Some functions not found...\n");
            return false;
        }
    }
    // find PDC if necessary; else set up server name for call


    byte* buf = 0;
    DWORD rc, rc2, read, total, i, rcdomain;
    std::array<char, MAXLEN * sizeof(wchar_t)> server;
    std::array<char, MAXLEN * sizeof(wchar_t)> user;
    std::array<char, MAXLEN * sizeof(wchar_t)> group;
    std::array<char, MAXLEN> groupname;
    std::array<char, MAXLEN * sizeof(wchar_t)> domain;

    std::size_t pnconv;
    if (isNT)
    {
        ::mbstowcs_s(&pnconv, (wchar_t*)user.data(), MAXLEN, userin, MAXLEN);
    }
    else
    {
        ::strcpy_s(user.data(), user.size(), userin);
    }

    if (isNT)
    {
        ::mbstowcs_s(&pnconv, (wchar_t*)group.data(), MAXLEN, groupin, MAXLEN);
    }
    else
    {
        ::strcpy_s(group.data(), group.size(), groupin);
    }
    ////////////////////////////////////////////////////////////////////////////////////////////
    ////NT
    ////////////////////////////////////////////////////////////////////////////////////////////
    if (isNT)
    {

        std::printf("Client NT4 or later \n");
        ///////////////// search DC ///////////////////////////////////
        rcdomain = NetGetDCNameNT(0, 0, &buf);
        if (rcdomain == 2453)
        {
            std::printf("No domain controler found\n");
        }
        if (!rcdomain)
        {
            std::printf("Domain controler found");
            ::wcscpy_s((wchar_t*)server.data(), MAXLEN, (wchar_t*)buf);
            wprintf((wchar_t*)server.data());
            std::printf("\n------------------------------\n");
        }
        std::printf("Checking Local groups for ");
        std::printf("user: %S\n", _wcsupr((wchar_t*)user.data()));
        std::printf("group: %S\n", _wcsupr((wchar_t*)group.data()));

        //////////////////////////////////////////////////////////////////
        ///////////////// Local groups ///////////////////////////////////
        //////////////////////////////////////////////////////////////////
        //////////////////////////////////////////////////////////////////
        if ((locdom == 1) || (locdom == 3))
        {
            LOCALGROUP_MEMBERS_INFO_2 *bufLMI, *cur;
            DWORD readInner, totalInner, resumeh, rcInner, iInner;
            std::array<wchar_t, MAXLEN> tempbuf;
            std::array<wchar_t, MAXLEN> seperator;

            netlocalgroupgetmembers_t netlocalgroupgetmembers;
            HINSTANCE h = ::LoadLibrary("netapi32.dll");
            netlocalgroupgetmembers = (netlocalgroupgetmembers_t)::GetProcAddress(h, "NetLocalGroupGetMembers");

            ////////////////////// GROUPS///////////////////////////////////////
            std::size_t pnconvInner;
            ::mbstowcs_s(&pnconvInner, seperator.data(), 256, "\\", 5);
            ///////////////////////GROUP1/////////////////////////////////////
            resumeh = 0;
            do
            {
                bufLMI = nullptr;
                rcInner = netlocalgroupgetmembers(nullptr, (wchar_t*)group.data(), 2, (BYTE**)&bufLMI, BUFSIZE, &readInner, &totalInner, &resumeh);
                if ((rcInner == ERROR_MORE_DATA) || (rcInner == ERROR_SUCCESS))
                {

                    for (iInner = 0, cur = bufLMI; iInner < readInner; ++iInner, ++cur)
                    {
                        // Note: the capital S in the format string will expect Unicode
                        // strings, as this is a program written/compiled for ANSI.
                        _wcsupr(cur->lgrmi2_domainandname);
                        ::wcscpy_s(tempbuf.data(), tempbuf.size(), cur->lgrmi2_domainandname);
                        wchar_t* t = std::wcsstr(tempbuf.data(), seperator.data());
                        ++t;
                        std::printf("%S\n", t);
                        if (std::wcscmp(_wcsupr(t), _wcsupr((wchar_t*)user.data())) == 0)
                        {
                            laccess_vnc = TRUE;
                            std::printf("Local: User found in group \n");
                        }
                    }
                    if (bufLMI != nullptr)
                    {
                        NetApiBufferFree(bufLMI);
                    }
                }
            } while (rcInner == ERROR_MORE_DATA);

            ///////////////////////////////////////////////////////////////////////
            if (h != 0)
            {
                ::FreeLibrary(h);
            }
        }


        if (!rcdomain)
        {
            DWORD rcInner;
            std::printf("New added ----  just for testing \n");
            ::wcscpy_s((wchar_t*)server.data(), MAXLEN, (wchar_t*)buf);
            byte* buf2 = 0;
            rc2 = NetWkstaGetInfoNT(0, 100, &buf2);
            std::size_t pnconvInner725;
            if (rc2)
            {
                std::printf("NetWkstaGetInfoA() returned %lu \n", rc2);
            }
            else
            {
                ::wcstombs_s(&pnconvInner725, domain.data(), MAXLEN, ((WKSTA_INFO_100_NT*)buf2)->wki100_langroup, MAXLEN);
            }
            NetApiBufferFree(buf2);
            domain[MAXLEN - 1] = '\0';
            std::printf("Detected domain = %s\n", domain.data());
            buf2 = 0;
            std::array<char, MAXLEN * sizeof(wchar_t)> userdomain;
            std::array<char, MAXLEN> userdom;
            ::strcpy_s(userdom.data(), userdom.size(), domain.data());
            ::strcat_s(userdom.data(), userdom.size(), "\\");
            ::strcat_s(userdom.data(), userdom.size(), userin);
            ::mbstowcs_s(&pnconvInner725, (wchar_t*)userdomain.data(), MAXLEN, userdom.data(), userdom.size());
            std::printf("%S\n", (wchar_t*)userdomain.data());
            rcInner = NetUserGetGroupsNT(nullptr, (wchar_t*)userdomain.data(), 0, 1, &buf2, MAX_PREFERRED_LENGTH, &read, &total);
            if (rcInner == NERR_Success)
            {
                for (i = 0; i < read; ++i)
                {
                    std::size_t pnconvInner;
                    ::wcstombs_s(&pnconvInner, groupname.data(), groupname.size(), ((LPLOCALGROUP_USERS_INFO_0_NT*)buf2)[i].grui0_name, MAXLEN);
                    groupname[MAXLEN - 1] = '\0'; // because strncpy won't do this if overflow
#ifdef _MSC_VER
                    _strupr(groupname.data());
                    _strupr(groupin);
#else
                    _strupr(groupname.data());
                    _strupr(groupin);
#endif
                    std::printf("compare %s %s\n", groupname.data(), group.data());
                    if (std::strcmp(groupname.data(), groupin) == 0)
                    {
                        std::printf("match...\n");
                        laccess_vnc = TRUE;
                    }
                    else
                    {
                        std::printf("no match...\n");
                    }
                }
                if (laccess_vnc == TRUE)
                {
                    std::printf("group found...\n");
                }
                else
                {
                    std::printf("No group found \n");
                }
            }
            NetApiBufferFree(buf2);
        }


        if ((locdom == 2) || (locdom == 3))
            if (!rcdomain)
            {
                DWORD rcInner;
                std::printf("New added ----  just for testing \n");
                ::wcscpy_s((wchar_t*)server.data(), MAXLEN, (wchar_t*)buf);
                byte* buf2 = 0;
                rc2 = NetWkstaGetInfoNT(0, 100, &buf2);
                std::size_t pnconvInner792;
                if (rc2)
                {
                    std::printf("NetWkstaGetInfoA() returned %lu \n", rc2);
                }
                else
                {
                    ::wcstombs_s(&pnconvInner792, domain.data(), MAXLEN, ((WKSTA_INFO_100_NT*)buf2)->wki100_langroup, MAXLEN);
                }
                NetApiBufferFree(buf2);
                domain[MAXLEN - 1] = '\0';
                std::printf("Detected domain = %s\n", domain.data());
                buf2 = 0;
                std::array<char, MAXLEN * sizeof(wchar_t)> userdomain;
                std::array<char, MAXLEN> userdom;
                //strcpy(_s(userdom,domain);
                //strcat_s(userdom,"\\");
                ::strcpy_s(userdom.data(), userdom.size(), userin);
                ::mbstowcs_s(&pnconvInner792, (wchar_t*)userdomain.data(), MAXLEN, userdom.data(), userdom.size());
                std::printf("%S\n", (wchar_t*)userdomain.data());
                rcInner = NetUserGetGroupsNT2((wchar_t*)server.data(), (wchar_t*)userdomain.data(), 0, &buf2, MAX_PREFERRED_LENGTH,
                                         &read, &total);
                if (rcInner == NERR_Success)
                {
                    for (i = 0; i < read; ++i)
                    {
                        std::size_t pnconvInner;
                        ::wcstombs_s(&pnconvInner, groupname.data(), groupname.size(), ((LPLOCALGROUP_USERS_INFO_0_NT*)buf2)[i].grui0_name, MAXLEN);
                        groupname[MAXLEN - 1] = '\0'; // because strncpy won't do this if overflow
#ifdef _MSC_VER
                        _strupr(groupname.data());
                        _strupr(groupin);
#else
                        _strupr(groupname.data());
                        _strupr(groupin);
#endif
                        std::printf("compare %s %s\n", groupname.data(), group.data());
                        if (std::strcmp(groupname.data(), groupin) == 0)
                        {
                            std::printf("match...\n");
                            laccess_vnc = TRUE;
                        }
                        else
                        {
                            std::printf("no match...\n");
                        }
                    }
                    if (laccess_vnc == TRUE)
                    {
                        std::printf("group found...\n");
                    }
                    else
                    {
                        std::printf("No group found \n");
                    }
                }
                NetApiBufferFree(buf2);
            }
    }
    ////////////////////////////////////////////////////////////////////////////////////////////
    ////9.X
    ////////////////////////////////////////////////////////////////////////////////////////////
    if ((locdom == 2) || (locdom == 3))
        if (!isNT)
        {
            rc = NetGetDCName95(0, &buf);
            if (rc)
            {
                std::printf("NetGetDCName95() returned %lu\n", rc);
            }
            if (rc == 2453)
            {
                std::printf("No domain controler found\n");
                return false;
            }
            if (!rc)
            {
                ::strcpy_s(server.data(), server.size(), (char*)buf);
                byte* buf2 = 0;
                rc = NetWkstaGetInfo95(0, 100, &buf2);
                if (rc)
                {
                    std::printf("NetWkstaGetInfoA() returned %lu \n", rc);
                }
                else
                {
                    ::strncpy_s(domain.data(), domain.size(), ((WKSTA_INFO_100_95*)buf2)->wki100_langroup, MAXLEN);
                }
                NetApiBufferFree(buf2);
                domain[MAXLEN - 1] = '\0';
                NetApiBufferFree(buf);
                buf = 0;
                buf2 = 0;
                ////////////////////////////////////////////
                rc = NetUserGetGroups95(server.data(), user.data(), 0, &buf2, 2024, &read, &total);
                if (rc == NERR_Success)
                {
                    for (i = 0; i < read; ++i)
                    {
                        ::strncpy_s(groupname.data(), groupname.size(), ((LPLOCALGROUP_USERS_INFO_0_95*)buf2)[i].grui0_name, MAXLEN);
                        groupname[MAXLEN - 1] = '\0'; // because strncpy won't do this if overflow
#ifdef _MSC_VER
                        _strupr(groupname.data());
#else
                        _strupr(groupname.data());
#endif
                        std::printf("%s\n", groupname.data());
                        if (std::strcmp(groupname.data(), group.data()) == 0)
                        {
                            laccess_vnc = TRUE;
                        }
                    }
                    if (access_vnc == TRUE)
                    {
                        std::printf("NT: Domain group found...\n");
                    }
                    else
                    {
                        std::printf("No Domain group found \n");
                    }
                }
                NetApiBufferFree(buf2);
                buf2 = 0;
            }
        }


    if (buf)
    {
        NetApiBufferFree(buf);
    }
    if (hNet != 0)
    {
        ::FreeLibrary(hNet);
    }
    if (hLoc != 0)
    {
        ::FreeLibrary(hLoc);
    }
    //check user
    /*if (isNT)
    {
    size_t pnconv;
#if _MSC_VER < 1400
        wcstombs_s( &pnconv, user, MAXLEN, (unsigned short *)user, MAXLEN);
#else
        wcstombs_s(&pnconv,  user, MAXLEN, (const wchar_t *)user, MAXLEN);
#endif
    }*/

    ::strcpy_s(user.data(), user.size(), userin);

    BOOL logon_OK{ false };
    BOOL passwd_OK{ false };
#ifdef _MSC_VER
    _strupr(user.data());
#else
    _strupr(user.data());
#endif
    passwd_OK = false;
    if (isXP)
    {
        LOGONUSER LU = nullptr;
        HMODULE shell1 = ::LoadLibrary("advapi32.dll");
        if (shell1 != nullptr)
        {
            LU = (LOGONUSER)::GetProcAddress((HINSTANCE)shell1, "LogonUserA");
        }
        if (LU != nullptr)
        {
            HANDLE token;
            if ((locdom == 2) || (locdom == 3) || (locdom == 1))
                if (LU(user.data(), domain.data(), password, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &token))
                {
                    passwd_OK = true;
                    if (passwd_OK == TRUE)
                    {
                        std::printf("Domain password check OK \n");
                    }
                    else
                    {
                        std::printf("Domain password check Failed \n");
                    }
                }
            if ((locdom == 1) || (locdom == 3))
                if (LU(user.data(), ".", password, LOGON32_LOGON_INTERACTIVE, LOGON32_PROVIDER_DEFAULT, &token))
                {
                    passwd_OK = true;
                    if (passwd_OK == TRUE)
                    {
                        std::printf("Local password check OK \n");
                    }
                }
        }
        ::FreeLibrary(shell1);
    }
    else
    {
        if ((locdom == 2) || (locdom == 3) || (locdom == 1))
            if (SSPLogonUser(domain.data(), user.data(), password))
            {
                passwd_OK = true;
                if (passwd_OK == TRUE)
                {
                    std::printf("Domain password check OK \n");
                }
            }
            else
            {
                std::printf("Domain password check Failed \n");
            }

        if ((locdom == 1) || (locdom == 3))
            if (SSPLogonUser(".", user.data(), password))
            {
                passwd_OK = true;
                if (passwd_OK == TRUE)
                {
                    std::printf("Local password check OK \n");
                }
            }
            else
            {
                std::printf("Local password check Failed \n");
            }
        if ((locdom == 2) || (locdom == 3))
            if (SSPLogonUser(domain.data(), "isdiua", "hegbfsa"))
            {
                passwd_OK = false;
                std::printf("Guest account block \n");
            }
        if ((locdom == 1) || (locdom == 3))
            if (SSPLogonUser(".", "isdiua", "hegbfsa"))
            {
                passwd_OK = false;
                std::printf("Guest account block \n");
            }
    }
    if ((access_vnc == TRUE) || (laccess_vnc == TRUE) || (ad_access == TRUE))
    {
        if (passwd_OK)
        {
            logon_OK = true;
            std::printf("Acces to vnc  OK \n");
        }
    }
    return logon_OK;
}
