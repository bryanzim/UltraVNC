// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.


#pragma warning(push)
#pragma warning(disable : 4996)
#include "cadthread.h"

#include "credentials.h"
#include "Localization.h"
#include "SettingsManager.h"

#include <array>
#include <cstring>

vncCad::vncCad() {}

bool vncCad::ISUACENabled()
{
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    if (OSversion.dwMajorVersion < 6)
    {
        return false;
    }
    HKEY hKey;
    if (::RegOpenKeyW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", &hKey) ==
        ERROR_SUCCESS)
    {
        DWORD value{ 0U };
        DWORD tt{ 4U };
        if (::RegQueryValueExW(hKey, L"EnableLUA", nullptr, nullptr, (LPBYTE)&value, &tt) == ERROR_SUCCESS)
        {
            ::RegCloseKey(hKey);
            return (value != 0);
        }
        ::RegCloseKey(hKey);
    }
    return false;
}

void vncCad::Enable_softwareCAD()
{
    HKEY hkLocal, hkLocalKey;
    DWORD dw;
    if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                       REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
    {
        return;
    }

    if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_WRITE | KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
    {
        ::RegCloseKey(hkLocal);
        return;
    }
    LONG pref;
    pref = 1;
    ::RegSetValueEx(hkLocalKey, "SoftwareSASGeneration", 0, REG_DWORD, (LPBYTE)&pref, sizeof(pref));
    ::RegCloseKey(hkLocalKey);
    ::RegCloseKey(hkLocal);
}

void vncCad::delete_softwareCAD()
{
    //Beep(1000,10000);
    HKEY hkLocal, hkLocalKey;
    DWORD dw;
    if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                       REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
    {
        return;
    }
    if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_WRITE | KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
    {
        ::RegCloseKey(hkLocal);
        return;
    }
    ::RegDeleteValue(hkLocalKey, "SoftwareSASGeneration");
    ::RegCloseKey(hkLocalKey);
    ::RegCloseKey(hkLocal);
}

void vncCad::delete_softwareCAD_elevated()
{
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    if (OSversion.dwMajorVersion < 6)
    {
        return;
    }

    std::array<char, MAX_PATH> exe_file_name;
    ::GetModuleFileName(0, exe_file_name.data(), exe_file_name.size());
    SHELLEXECUTEINFO shExecInfo;
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = 0;
    shExecInfo.hwnd = ::GetForegroundWindow();
    shExecInfo.lpVerb = "runas";
    shExecInfo.lpFile = exe_file_name.data();
    shExecInfo.lpParameters = "-delsoftwarecad";
    shExecInfo.lpDirectory = nullptr;
    shExecInfo.nShow = SW_SHOWNORMAL;
    shExecInfo.hInstApp = nullptr;
    ::ShellExecuteEx(&shExecInfo);
}

void vncCad::Enable_softwareCAD_elevated()
{
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    if (OSversion.dwMajorVersion < 6)
    {
        return;
    }
    std::array<char, MAX_PATH> exe_file_name;
    ::GetModuleFileName(0, exe_file_name.data(), exe_file_name.size());
    SHELLEXECUTEINFO shExecInfo;
    shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
    shExecInfo.fMask = 0;
    shExecInfo.hwnd = ::GetForegroundWindow();
    shExecInfo.lpVerb = "runas";
    shExecInfo.lpFile = exe_file_name.data();
    shExecInfo.lpParameters = "-softwarecad";
    shExecInfo.lpDirectory = nullptr;
    shExecInfo.nShow = SW_SHOWNORMAL;
    shExecInfo.hInstApp = nullptr;
    ::ShellExecuteEx(&shExecInfo);
}

bool vncCad::IsSoftwareCadEnabled()
{
    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    if (OSversion.dwMajorVersion < 6)
    {
        return true;
    }

    HKEY hkLocal, hkLocalKey;
    DWORD dw;
    if (::RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies", 0, REG_NONE,
                       REG_OPTION_NON_VOLATILE, KEY_READ, nullptr, &hkLocal, &dw) != ERROR_SUCCESS)
    {
        return 0;
    }
    if (::RegOpenKeyEx(hkLocal, "System", 0, KEY_READ, &hkLocalKey) != ERROR_SUCCESS)
    {
        ::RegCloseKey(hkLocal);
        return 0;
    }

    LONG pref{ 0 };
    ULONG type = REG_DWORD;
    ULONG prefsize = sizeof(pref);

    if (RegQueryValueEx(hkLocalKey, "SoftwareSASGeneration", nullptr, &type, (LPBYTE)&pref, &prefsize) != ERROR_SUCCESS)
    {
        ::RegCloseKey(hkLocalKey);
        ::RegCloseKey(hkLocal);
        return false;
    }
    ::RegCloseKey(hkLocalKey);
    ::RegCloseKey(hkLocal);
    if (pref != 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

DWORD WINAPI vncCad::Cadthread(const LPVOID lpParam)
{
    //SET THREAD in CURRENT INPUTDESKTOP
    HANDLE hShutdownEventcad;
    hShutdownEventcad = ::OpenEvent(EVENT_MODIFY_STATE, FALSE, "Global\\SessionEventUltraCad");
    //Switch to InputDesktop
    HDESK desktop = nullptr;
    desktop = ::OpenInputDesktop(0, FALSE,
                               DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                   DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
    HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
    ::SetThreadDesktop(desktop);

    OSVERSIONINFO OSversion;
    OSversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&OSversion);
    //
    if ((OSversion.dwMajorVersion >= 6) && settings->RunningFromExternalService() && !IsSoftwareCadEnabled())
    {
        DWORD result = MessageBoxSecure(nullptr, sz_ID_UAC_DISABLED_REGISTRY.data(), sz_ID_WARNING_CAPTION.data(), MB_YESNO);
        if (result == IDYES)
        {
            HANDLE hPToken = DesktopUsersToken::getInstance()->getDesktopUsersToken();
            if (hPToken)
            {
                std::array<char, MAX_PATH + 32> dir;
                std::array<char, MAX_PATH> exe_file_name;
                ::GetModuleFileName(0, exe_file_name.data(), exe_file_name.size());
                ::sprintf_s(dir.data(), dir.size(), "\"%s\" -softwarecadhelper", exe_file_name.data());

                STARTUPINFO StartUPInfo{};
                PROCESS_INFORMATION ProcessInfo{};
                ::ZeroMemory(&StartUPInfo, sizeof(STARTUPINFO));
                ::ZeroMemory(&ProcessInfo, sizeof(PROCESS_INFORMATION));
                StartUPInfo.wShowWindow = SW_SHOW;
                StartUPInfo.lpDesktop = "Winsta0\\Default";
                StartUPInfo.cb = sizeof(STARTUPINFO);
                ::CreateProcessAsUser(hPToken, nullptr, dir.data(), nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr, nullptr,
                                    &StartUPInfo, &ProcessInfo);

                DWORD errorcode = ::GetLastError();
                if (ProcessInfo.hProcess)
                {
                    ::CloseHandle(ProcessInfo.hProcess);
                }
                if (ProcessInfo.hThread)
                {
                    ::CloseHandle(ProcessInfo.hThread);
                }
                if (errorcode == 1314)
                {
                    goto error;
                }
                goto gotome;
            }
        error:
            Enable_softwareCAD_elevated();
        }
    }
gotome:

    //Tell service to call sendSas()
    if (OSversion.dwMajorVersion >= 6)
    {
        if (hShutdownEventcad == nullptr)
        {
            hShutdownEventcad = ::OpenEvent(EVENT_MODIFY_STATE, FALSE, "Global\\SessionEventUltraCad");
        }
        if (hShutdownEventcad != nullptr)
        {
            ::SetEvent(hShutdownEventcad);
        }
    }
    else
    { // call cad.exe
        std::array<char, MAX_PATH> WORKDIR;
        std::array<char, MAX_PATH> mycommand;
        if (::GetModuleFileName(nullptr, WORKDIR.data(), WORKDIR.size()))
        {
            char* p = std::strrchr(WORKDIR.data(), '\\');
            if (p == nullptr)
            {
                return 0;
            }
            *p = '\0';
        }
        ::strcpy_s(mycommand.data(), mycommand.size(), "");
        ::strcat_s(mycommand.data(), mycommand.size(), WORKDIR.data()); //set the directory
        ::strcat_s(mycommand.data(), mycommand.size(), "\\");
        ::strcat_s(mycommand.data(), mycommand.size(), "cad.exe");
        ::ShellExecute(::GetDesktopWindow(), "open", mycommand.data(), "", 0, SW_SHOWNORMAL);
    }
    if (hShutdownEventcad)
    {
        ::CloseHandle(hShutdownEventcad);
    }
    if (old_desktop)
    {
        ::SetThreadDesktop(old_desktop);
    }
    if (desktop)
    {
        ::CloseDesktop(desktop);
    }
    return 0;
}
#pragma warning(pop)