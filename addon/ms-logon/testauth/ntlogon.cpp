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


#include <Windows.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <stdio.h>
#include <stdlib.h>

typedef BOOL (*CheckUserGroupPasswordFn)(char* userin, char* password, char* machine, char* group, int lodom);
CheckUserGroupPasswordFn CheckUserGroupPassword = nullptr;
bool CheckAD()
{
    HMODULE hModule = ::LoadLibrary("Activeds.dll");
    if (hModule)
    {
        ::FreeLibrary(hModule);
        return true;
    }
    return false;
}

bool CheckNetapi95()
{
    HMODULE hModule = ::LoadLibrary("netapi32.dll");
    if (hModule)
    {
        ::FreeLibrary(hModule);
        return true;
    }
    return false;
}

bool CheckDsGetDcNameW()
{
    HMODULE hModule = ::LoadLibrary("netapi32.dll");
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
    HMODULE hModule = ::LoadLibrary("radmin32.dll");
    if (hModule)
    {
        ::FreeLibrary(hModule);
        return true;
    }
    return false;
}

int main(const int argc, char* argv[])
{
    std::array<char, 256U> pszBuffer;
    std::array<char, 256U> pszBuffer2;
    std::array<char, 256U> pszBuffer3;
    std::array<char, 256U> pszBuffer4;
    int locdom;
    if (argc < 5)
    {
        std::printf("Enter user name : ");
        fgets(pszBuffer.data(), 256, stdin);
        std::printf("\nEnter password : ");
        fgets(pszBuffer2.data(), 256, stdin);
        std::printf("\n");
        std::printf("Enter group : ");
        fgets(pszBuffer3.data(), 256, stdin);
        std::printf("\n");
        std::printf("loc=1 dom=3 local+domain=3 : ");
        fgets(pszBuffer4.data(), 256, stdin);
        std::printf("\n");
        if (0 == std::strcmp("", pszBuffer.data()))
        {
            return 0;
        }
        if (0 == std::strcmp("", pszBuffer2.data()))
        {
            return 0;
        }
        if (0 == std::strcmp("", pszBuffer3.data()))
        {
            return 0;
        }
        if (0 == std::strcmp("", pszBuffer4.data()))
        {
            return 0;
        }
    }
    else
    {
        ::strcpy_s(pszBuffer.data(), pszBuffer.size(), argv[1U]);
        ::strcpy_s(pszBuffer2.data(), pszBuffer2.size(), argv[2U]);
        ::strcpy_s(pszBuffer3.data(), pszBuffer3.size(), argv[3U]);
        ::strcpy_s(pszBuffer4.data(), pszBuffer4.size(), argv[4U]);
    }
    locdom = atoi(pszBuffer4.data());
    if (!CheckNetapi95() && !CheckNetApiNT())
    {
        std::printf("Netapi not found,radmin32.dll is missing \n");
    }

    ///////////////////////////////////////
    ////// BASIC LOCAL GROUPS
    BOOL returnvalue;
    {
        returnvalue = false;
        std::printf("\nTrying authlogonuser.dll \n");
        std::printf("This only works on Windows XP> \n");
        std::printf("test is runnning as application and not a service \n");
        std::printf("------------------------ \n");
        std::printf("------------------------ \n");
        HMODULE hModule = ::LoadLibrary("authlogonuser.dll");
        if (hModule)
        {
            CheckUserGroupPassword = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModule, "CUGP"));
            [[maybe_unused]] HRESULT hr = ::CoInitialize(nullptr);
            returnvalue = ::CheckUserGroupPassword(pszBuffer.data(), pszBuffer2.data(), "test", pszBuffer3.data(), locdom);
            ::CoUninitialize();
            ::FreeLibrary(hModule);
        }
        else
        {
            std::printf("authlogonuser.dll not found");
        }
        if (returnvalue == 1)
        {
            std::printf("based on auth.dll user has access ");
        }
        else
        {
            std::printf("based on authlogonuser.dll user has NO access ");
        }
    }
    if (returnvalue == 1)
    {
        std::printf("\n-------------------------\n");
    }
    if (returnvalue == 1)
    {
        std::printf("For testing we cont, but access was already granted\n");
    }
    if (returnvalue == 1)
    {
        std::printf("-------------------------\n");
    }

    returnvalue = false;
    std::printf("\nTrying auth.dll \n");
    std::printf("------------------------ \n");
    std::printf("------------------------ \n");
    HMODULE hModule = ::LoadLibrary("auth.dll");
    if (hModule)
    {
        CheckUserGroupPassword = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModule, "CUGP"));
        [[maybe_unused]] HRESULT hr = ::CoInitialize(nullptr);
        returnvalue = ::CheckUserGroupPassword(pszBuffer.data(), pszBuffer2.data(), "test", pszBuffer3.data(), locdom);
        ::CoUninitialize();
        ::FreeLibrary(hModule);
    }
    else
    {
        std::printf("auth.dll not found");
    }
    if (returnvalue == 1)
    {
        std::printf("based on auth.dll user has access ");
    }
    else
    {
        std::printf("based on auth.dll user has NO access ");
    }
    if (returnvalue == 1)
    {
        std::printf("\n-------------------------\n");
    }
    if (returnvalue == 1)
    {
        std::printf("For testing we cont, but access was already granted\n");
    }
    if (returnvalue == 1)
    {
        std::printf("-------------------------\n");
    }
    returnvalue = false;
    if (CheckAD())
    {
        std::printf("\nTrying authad.dll \n");
        std::printf("------------------------ \n");
        std::printf("------------------------ \n");
        HMODULE hModuleInner = ::LoadLibrary("authad.dll");
        if (hModuleInner)
        {
            CheckUserGroupPassword = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleInner, "CUGP"));
            [[maybe_unused]] HRESULT hr = ::CoInitialize(nullptr);
            returnvalue = ::CheckUserGroupPassword(pszBuffer.data(), pszBuffer2.data(), "test", pszBuffer3.data(), locdom);
            ::CoUninitialize();
            ::FreeLibrary(hModuleInner);
        }
        else
        {
            std::printf("authad.dll not found");
        }
        if (returnvalue == 1)
        {
            std::printf("based on authad.dll user has access ");
        }
        else
        {
            std::printf("based on authad.dll user has NO access ");
        }
    }
    if (returnvalue == 1)
    {
        std::printf("\n-------------------------\n");
    }
    if (returnvalue == 1)
    {
        std::printf("For testing we cont, but access was already granted\n");
    }
    if (returnvalue == 1)
    {
        std::printf("-------------------------\n");
    }
    if ((locdom == 2) || (locdom == 3))
    {
        returnvalue = false;
        OSVERSIONINFO VerInfo;
        VerInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!GetVersionEx(&VerInfo)) // If this fails, something has gone wrong
        {
            return FALSE;
        }
        if ((VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) && (VerInfo.dwMajorVersion == 4))
        {
            if (CheckAD() && CheckDsGetDcNameW())
            {
                std::printf("\nTrying ldapauthNT4.dll \n");
                std::printf("------------------------ \n");
                std::printf("------------------------ \n");
                HMODULE hModuleInner = ::LoadLibrary("ldapauthnt4.dll");
                if (hModuleInner)
                {
                    CheckUserGroupPassword = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleInner, "CUGP"));
                    [[maybe_unused]] HRESULT hr = ::CoInitialize(nullptr);
                    returnvalue = ::CheckUserGroupPassword(pszBuffer.data(), pszBuffer2.data(), "test", pszBuffer3.data(), locdom);
                    ::CoUninitialize();
                    ::FreeLibrary(hModuleInner);
                }
                else
                {
                    std::printf("ldapauthnt4.dll not found");
                }
                if (returnvalue == 1)
                {
                    std::printf("based on ldapauthnt4.dll user has access ");
                }
                else
                {
                    std::printf("based on ldapauthnt4.dll user has NO access ");
                }
            }
        }
        if (returnvalue == 1)
        {
            std::printf("\n-------------------------\n");
        }
        if (returnvalue == 1)
        {
            std::printf("For testing we cont, but access was already granted\n");
        }
        if (returnvalue == 1)
        {
            std::printf("-------------------------\n");
        }
        returnvalue = false;
        if ((VerInfo.dwPlatformId == VER_PLATFORM_WIN32_NT) && (VerInfo.dwMajorVersion >= 5))
        {
            if (CheckAD())
            {
                std::printf("\nTrying ldapauth.dll \n");
                std::printf("------------------------ \n");
                std::printf("------------------------ \n");
                HMODULE hModuleInner = ::LoadLibrary("ldapauth.dll");
                if (hModuleInner)
                {
                    CheckUserGroupPassword = reinterpret_cast<CheckUserGroupPasswordFn>(::GetProcAddress(hModuleInner, "CUGP"));
                    [[maybe_unused]] HRESULT hr = ::CoInitialize(nullptr);
                    returnvalue = ::CheckUserGroupPassword(pszBuffer.data(), pszBuffer2.data(), "test", pszBuffer3.data(), locdom);
                    ::CoUninitialize();
                    ::FreeLibrary(hModuleInner);
                }
                else
                {
                    std::printf("ldapauth.dll not found");
                }
                if (returnvalue == 1)
                {
                    std::printf("based on ldapauth.dll user has access ");
                }
                else
                {
                    std::printf("based on ldapauth.dll user has NO access ");
                }
            }
        }
    }
    std::printf("Enter to quit ");
    fgets(pszBuffer.data(), 256, stdin);
    return 0;
}
