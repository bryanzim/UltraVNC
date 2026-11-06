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

#include "stdhdrs.h"

#include "omnithread.h"
#include "SettingsManager.h"
// Marscha@2004 - authSSP: from stdhdrs.h, required for logging
#include "vnclog.h"
#include "winvnc.h"

#include <Windows.h>

#include <objbase.h>

#include <array>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

extern VNCLog vnclog;
#include "common/inifile.h"
// Marscha@2004 - authSSP: end of change
#include "Localization.h" // Act : add localization on messages

typedef BOOL (*CheckUserGroupPasswordFn)(char* userin, char* password, char* machine, char* group, int locdom);

int CheckUserGroupPasswordUni(char* userin, char* password, const char* machine);
int CheckUserGroupPasswordUni2(char* userin, char* password, const char* machine);

// Marscha@2004 - authSSP: if "New MS-Logon" is checked, call CheckUserPasswordSDUni
BOOL IsNewMSLogon();
//char *AddToModuleDir(char *filename, int length);
typedef int (*CheckUserPasswordSDUniFn)(const char* userin, const char* password, const char* machine, TCHAR* szMslogonLog);
CheckUserPasswordSDUniFn CheckUserPasswordSDUni = nullptr;

#define MAXSTRING 254

///////////////////////////////////////////////////////////
bool CheckAD()
{
    HMODULE hModule = ::LoadLibraryEx("Activeds.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hModule)
    {
        ::FreeLibrary(hModule);
        return true;
    }
    return false;
}

bool CheckNetapi95()
{
    HMODULE hModule = ::LoadLibraryEx("netapi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hModule)
    {
        ::FreeLibrary(hModule);
        return true;
    }
    return false;
}

bool CheckDsGetDcNameW()
{
    HMODULE hModule = ::LoadLibraryEx("netapi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hModule)
    {
        FARPROC test = nullptr;
        test = ::GetProcAddress(hModule, "DsGetDcNameW");
        ::FreeLibrary(hModule);
        if (test)
        {
            return true;
        }
    }
    return false;
}

bool CheckNetApiNT()
{
    HMODULE hModule = ::LoadLibraryEx("radmin32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hModule)
    {
        ::FreeLibrary(hModule);
        return true;
    }
    return false;
}

int CheckUserGroupPasswordUni(char* const userin, char* const password, const char* const machine)
{
    int result{ 0 };
    HMODULE hModuleAuthSSP = nullptr;
    // Marscha@2004 - authSSP: if "New MS-Logon" is checked, call CUPSDV2 in authSSP.dll,
    // else call "old" MS-Logon method.
    if (IsNewMSLogon())
    {
        std::array<char, MAX_PATH> szCurrentDir;
        ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
        ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\authSSP.dll");

        hModuleAuthSSP = ::LoadLibrary(szCurrentDir.data());
        if (hModuleAuthSSP)
        {
            CheckUserPasswordSDUni = reinterpret_cast<CheckUserPasswordSDUniFn>(::GetProcAddress(hModuleAuthSSP, "CUPSDV2"));
            vnclog.Print(LL_INTINFO, VNCLOG("GetProcAddress"));
            ::CoInitialize(nullptr);
            result = ::CheckUserPasswordSDUni(userin, password, machine, settings->getLogFile());
            vnclog.Print(LL_INTINFO, "CheckUserPasswordSDUni result=%i", result);
            ::CoUninitialize();
            ::FreeLibrary(hModuleAuthSSP);
            //result = CheckUserPasswordSDUni(userin, password, machine);
        }
        else
        {
            LPCTSTR sz_ID_AUTHSSP_NOT_FO = // to be moved to localization.h
                "You selected MS-Logon, but authSSP.dll\nwas not found.Check you installation";
            MessageBoxSecure(nullptr, sz_ID_AUTHSSP_NOT_FO, sz_ID_WARNING.data(), MB_OK);
        }
    }
    else
    {
        result = CheckUserGroupPasswordUni2(userin, password, machine);
    }
    return result;
}

int CheckUserGroupPasswordUni2(char* const userin, char* const password, const char* const machine)
{
    HMODULE hModuleAdmin = nullptr;
    HMODULE hModuleWorkgroup = nullptr;
    HMODULE hModuleLdapAuth = nullptr;
    HMODULE hModuleLdapAuthNT4 = nullptr;
    HMODULE hModuleLdapAuth9x = nullptr;

    CheckUserGroupPasswordFn CheckUserGroupPasswordAdmin = nullptr;
    CheckUserGroupPasswordFn CheckUserGroupPasswordWorkgroup = nullptr;
    CheckUserGroupPasswordFn CheckUserGroupPasswordLdapAuth = nullptr;
    CheckUserGroupPasswordFn CheckUserGroupPasswordLdapAuthNT4 = nullptr;
    CheckUserGroupPasswordFn CheckUserGroupPasswordLdapAuth9x = nullptr;

    int result{ 0 };
    BOOL NT4OS{ false };
    BOOL W2KOS{ false };
    std::array<char, 256U> clientname;
    ::strcpy_s(clientname.data(), 256, machine);
    if (!CheckNetapi95() && !CheckNetApiNT())
    {
        return false;
    }
    OSVERSIONINFO VerInfo{};
    VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (!GetVersionEx(&VerInfo)) // If this fails, something has gone wrong
    {
        return FALSE;
    }
    if ((VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) && (VerInfo.dwMajorVersion == 4))
    {
        NT4OS = true;
    }
    if ((VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) && (VerInfo.dwMajorVersion >= 5))
    {
        W2KOS = true;
    }

    std::array<char, MAX_PATH> szCurrentDir;
    ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
    ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\authadmin.dll");

    hModuleAdmin = ::LoadLibrary(szCurrentDir.data());
    if (hModuleAdmin)
    {
        CheckUserGroupPasswordAdmin = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleAdmin, "CUGP"));
    }

    ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
    ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\workgrpdomnt4.dll");

    hModuleWorkgroup = ::LoadLibrary(szCurrentDir.data());
    if (hModuleWorkgroup)
    {
        CheckUserGroupPasswordWorkgroup = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleWorkgroup, "CUGP"));
    }
    ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
    ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\ldapauth.dll");

    hModuleLdapAuth = ::LoadLibrary(szCurrentDir.data());
    if (hModuleLdapAuth)
    {
        CheckUserGroupPasswordLdapAuth = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleLdapAuth, "CUGP"));
    }
    ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
    ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\ldapauthnt4.dll");

    hModuleLdapAuthNT4 = ::LoadLibrary(szCurrentDir.data());
    if (hModuleLdapAuthNT4)
    {
        CheckUserGroupPasswordLdapAuthNT4 = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleLdapAuthNT4, "CUGP"));
    }
    ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
    ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\ldapauth9x.dll");

    hModuleLdapAuth9x = ::LoadLibrary(szCurrentDir.data());
    if (hModuleLdapAuth9x)
    {
        CheckUserGroupPasswordLdapAuth9x = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleLdapAuth9x, "CUGP"));
    }
    //////////////////////////////////////////////////
    // Load reg settings
    //////////////////////////////////////////////////

    //////////////////////////////////////////////////
    // logon user only works on Windows NT>
    // Windows NT4/Windows 2000 only as service (system account)
    // Windows XP> works also as application
    // Group is not used...admin access rights is needed
    // MS keep changes there security model for each version....
    //////////////////////////////////////////////////
    ////////////////////////////////////////////////////
    if ((std::strcmp(settings->getGroup1(), "") == 0) && (std::strcmp(settings->getGroup2(), "") == 0) &&
        (std::strcmp(settings->getGroup3(), "") == 0))
        if (NT4OS || W2KOS)
        {
            if (CheckUserGroupPasswordAdmin)
            {
                ::CoInitialize(nullptr);
                result =
                    CheckUserGroupPasswordAdmin(userin, password, clientname.data(), settings->getGroup1(), settings->getLocdom1());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_AUTHADMIN_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
    if (result == 1)
    {
        goto accessOK;
    }

    if (std::strcmp(settings->getGroup1(), "") != 0)
    {
        ///////////////////////////////////////////////////
        // NT4 domain and workgroups
        //
        ///////////////////////////////////////////////////
        {

            if (CheckUserGroupPasswordWorkgroup)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordWorkgroup(userin, password, clientname.data(), settings->getGroup1(),
                                                         settings->getLocdom1());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_WORKGRPDOMNT4_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        /////////////////////////////////////////////////////////////////
        if (CheckAD() && W2KOS && ((settings->getLocdom1() == 2) || (settings->getLocdom1() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuth)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuth(userin, password, clientname.data(), settings->getGroup1(),
                                                        settings->getLocdom1());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTH_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        //////////////////////////////////////////////////////////////////////
        if (CheckAD() && NT4OS && CheckDsGetDcNameW() && ((settings->getLocdom1() == 2) || (settings->getLocdom1() == 3)))
        {

            if (CheckUserGroupPasswordLdapAuthNT4)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuthNT4(userin, password, clientname.data(), settings->getGroup1(),
                                                           settings->getLocdom1());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTHNT4_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        //////////////////////////////////////////////////////////////////////
        if (CheckAD() && !NT4OS && !W2KOS && ((settings->getLocdom1() == 2) || (settings->getLocdom1() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuth9x)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuth9x(userin, password, clientname.data(), settings->getGroup1(),
                                                          settings->getLocdom1());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTH9X_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
    }
    /////////////////////////////////////////////////
    if (std::strcmp(settings->getGroup2(), "") != 0)
    {
        ///////////////////////////////////////////////////
        // NT4 domain and workgroups
        //
        ///////////////////////////////////////////////////
        {
            if (CheckUserGroupPasswordWorkgroup)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordWorkgroup(userin, password, clientname.data(), settings->getGroup2(),
                                                         settings->getLocdom2());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_WORKGRPDOMNT4_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        //////////////////////////////////////////////////////
        if (NT4OS || W2KOS)
        {
            if (CheckUserGroupPasswordAdmin)
            {
                ::CoInitialize(nullptr);
                result =
                    CheckUserGroupPasswordAdmin(userin, password, clientname.data(), settings->getGroup2(), settings->getLocdom2());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_AUTHADMIN_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        //////////////////////////////////////////////////////////////////
        if (CheckAD() && W2KOS && ((settings->getLocdom2() == 2) || (settings->getLocdom2() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuth)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuth(userin, password, clientname.data(), settings->getGroup2(),
                                                        settings->getLocdom2());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTH_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        ///////////////////////////////////////////////////////////////////////
        if (CheckAD() && NT4OS && CheckDsGetDcNameW() && ((settings->getLocdom2() == 2) || (settings->getLocdom2() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuthNT4)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuthNT4(userin, password, clientname.data(), settings->getGroup2(),
                                                           settings->getLocdom2());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTHNT4_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
        ///////////////////////////////////////////////////////////////////////
        if (CheckAD() && !NT4OS && !W2KOS && ((settings->getLocdom2() == 2) || (settings->getLocdom2() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuth9x)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuth9x(userin, password, clientname.data(), settings->getGroup2(),
                                                          settings->getLocdom2());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTH9X_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK;
        }
    }
    ////////////////////////////
    if (std::strcmp(settings->getGroup3(), "") != 0)
    {
        ///////////////////////////////////////////////////
        // NT4 domain and workgroups
        //
        ///////////////////////////////////////////////////
        {
            if (CheckUserGroupPasswordWorkgroup)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordWorkgroup(userin, password, clientname.data(), settings->getGroup3(),
                                                         settings->getLocdom3());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_WORKGRPDOMNT4_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK2;
        }
        ////////////////////////////////////////////////////////
        if (NT4OS || W2KOS)
        {
            if (CheckUserGroupPasswordAdmin)
            {
                ::CoInitialize(nullptr);
                result =
                    CheckUserGroupPasswordAdmin(userin, password, clientname.data(), settings->getGroup3(), settings->getLocdom3());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_AUTHADMIN_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK2;
        }
        ////////////////////////////////////////////////////////////////
        if (CheckAD() && W2KOS && ((settings->getLocdom3() == 2) || (settings->getLocdom3() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuth)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuth(userin, password, clientname.data(), settings->getGroup3(),
                                                        settings->getLocdom3());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTH_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK2;
        }
        ///////////////////////////////////////////////////////////////////
        if (CheckAD() && NT4OS && CheckDsGetDcNameW() && ((settings->getLocdom3() == 2) || (settings->getLocdom3() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuthNT4)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuthNT4(userin, password, clientname.data(), settings->getGroup3(),
                                                           settings->getLocdom3());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTHNT4_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK2;
        }
        ///////////////////////////////////////////////////////////////////
        if (CheckAD() && !NT4OS && !W2KOS && ((settings->getLocdom3() == 2) || (settings->getLocdom3() == 3)))
        {
            if (CheckUserGroupPasswordLdapAuth9x)
            {
                ::CoInitialize(nullptr);
                result = CheckUserGroupPasswordLdapAuth9x(userin, password, clientname.data(), settings->getGroup3(),
                                                          settings->getLocdom3());
                ::CoUninitialize();
            }
            else
            {
                MessageBoxSecure(nullptr, sz_ID_LDAPAUTH9X_DLL_NOT_FOUND.data(), sz_ID_WARNING.data(), MB_OK);
                result = 0;
            }
        }
        if (result == 1)
        {
            goto accessOK2;
        }
    }

    /////////////////////////////////////////////////
    // If we reach this place auth failed
    /////////////////////////////////////////////////
    {
        typedef BOOL (*LogeventFn)(char* machine, char* user, char* szMslogonLog);
        LogeventFn Logevent = nullptr;
        std::array<char, MAX_PATH> szCurrentDirInner;
        ::strcpy_s(szCurrentDirInner.data(), szCurrentDirInner.size(), winvncFolder.data());
        ::strcat_s(szCurrentDirInner.data(), szCurrentDirInner.size(), "\\logging.dll");

        HMODULE hModule = ::LoadLibrary(szCurrentDirInner.data());
        if (hModule)
        {
            Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGFAILEDUSERV2"));
            Logevent(reinterpret_cast<char*>(clientname.data()), userin, settings->getLogFile());
            ::FreeLibrary(hModule);
        }
        if (hModuleAdmin)
        {
            ::FreeLibrary(hModuleAdmin);
        }
        if (hModuleWorkgroup)
        {
            ::FreeLibrary(hModuleWorkgroup);
        }
        if (hModuleLdapAuth)
        {
            ::FreeLibrary(hModuleLdapAuth);
        }
        if (hModuleLdapAuthNT4)
        {
            ::FreeLibrary(hModuleLdapAuthNT4);
        }
        if (hModuleLdapAuth9x)
        {
            ::FreeLibrary(hModuleLdapAuth9x);
        }
        hModuleAdmin = nullptr;
        hModuleWorkgroup = nullptr;
        hModuleLdapAuth = nullptr;
        hModuleLdapAuthNT4 = nullptr;
        hModuleLdapAuth9x = nullptr;
        return result;
    }

accessOK: //full access
{
    typedef BOOL (*LogeventFn)(char* machine, char* user, char* szMslogonLog);
    LogeventFn Logevent = nullptr;
    std::array<char, MAX_PATH> szCurrentDirInner;
    ::strcpy_s(szCurrentDirInner.data(), szCurrentDirInner.size(), winvncFolder.data());
    ::strcat_s(szCurrentDirInner.data(), szCurrentDirInner.size(), "\\logging.dll");

    HMODULE hModule = ::LoadLibrary(szCurrentDirInner.data());
    if (hModule)
    {
        Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGLOGONUSERV2"));
        Logevent(reinterpret_cast<char*>(clientname.data()), userin, settings->getLogFile());
        ::FreeLibrary(hModule);
    }


    if (hModuleAdmin)
    {
        ::FreeLibrary(hModuleAdmin);
    }
    if (hModuleWorkgroup)
    {
        ::FreeLibrary(hModuleWorkgroup);
    }
    if (hModuleLdapAuth)
    {
        ::FreeLibrary(hModuleLdapAuth);
    }
    if (hModuleLdapAuthNT4)
    {
        ::FreeLibrary(hModuleLdapAuthNT4);
    }
    if (hModuleLdapAuth9x)
    {
        ::FreeLibrary(hModuleLdapAuth9x);
    }
    hModuleAdmin = nullptr;
    hModuleWorkgroup = nullptr;
    hModuleLdapAuth = nullptr;
    hModuleLdapAuthNT4 = nullptr;
    hModuleLdapAuth9x = nullptr;

    return result;
}

accessOK2: //readonly
{
    typedef BOOL (*LogeventFn)(char* machine, char* user, char* szMslogonLog);
    LogeventFn Logevent = nullptr;
    std::array<char, MAX_PATH> szCurrentDirInner;
    ::strcpy_s(szCurrentDirInner.data(), szCurrentDirInner.size(), winvncFolder.data());
    ::strcat_s(szCurrentDirInner.data(), szCurrentDirInner.size(), "\\logging.dll");

    HMODULE hModule = ::LoadLibrary(szCurrentDirInner.data());
    if (hModule)
    {
        Logevent = reinterpret_cast<LogeventFn>(::GetProcAddress(hModule, "LOGLOGONUSERV2"));
        Logevent(reinterpret_cast<char*>(clientname.data()), userin, settings->getLogFile());
        ::FreeLibrary(hModule);
    }
    if (hModuleAdmin)
    {
        ::FreeLibrary(hModuleAdmin);
    }
    if (hModuleWorkgroup)
    {
        ::FreeLibrary(hModuleWorkgroup);
    }
    if (hModuleLdapAuth)
    {
        ::FreeLibrary(hModuleLdapAuth);
    }
    if (hModuleLdapAuthNT4)
    {
        ::FreeLibrary(hModuleLdapAuthNT4);
    }
    if (hModuleLdapAuth9x)
    {
        ::FreeLibrary(hModuleLdapAuth9x);
    }
    hModuleAdmin = nullptr;
    hModuleWorkgroup = nullptr;
    hModuleLdapAuth = nullptr;
    hModuleLdapAuthNT4 = nullptr;
    hModuleLdapAuth9x = nullptr;
    result = 2;
}
    if (hModuleAdmin)
    {
        ::FreeLibrary(hModuleAdmin);
    }
    if (hModuleWorkgroup)
    {
        ::FreeLibrary(hModuleWorkgroup);
    }
    if (hModuleLdapAuth)
    {
        ::FreeLibrary(hModuleLdapAuth);
    }
    if (hModuleLdapAuthNT4)
    {
        ::FreeLibrary(hModuleLdapAuthNT4);
    }
    if (hModuleLdapAuth9x)
    {
        ::FreeLibrary(hModuleLdapAuth9x);
    }
    hModuleAdmin = nullptr;
    hModuleWorkgroup = nullptr;
    hModuleLdapAuth = nullptr;
    hModuleLdapAuthNT4 = nullptr;
    hModuleLdapAuth9x = nullptr;
    return result;
}

// Marscha@2004 - authSSP: Is New MS-Logon activated?
BOOL IsNewMSLogon() { return settings->getNewMSLogon(); }
