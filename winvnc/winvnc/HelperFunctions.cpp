// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "stdhdrs.h"

#include "common/inifile.h"
#include "common/win32_helpers.h"
#include "credentials.h"
#include "SettingsManager.h"
#include "UltraVNCService.h"

#include <winvnc/winvnc.h>

#include <lmcons.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstring>
#include <string>
//  We first use shellexecute with "runas"
//  This way we can use UAC and user/passwd
//	Runas is standard OS, so no security risk

HWND G_MENU_HWND = nullptr;
char* MENU_CLASS_NAME = "WinVNC Tray Icon";
bool ClientTimerReconnect{ false };
bool allowMultipleInstances{ false };
extern HINSTANCE hInstResDLL;

void Open_homepage() { ::ShellExecute(nullptr, "open", "https://uvnc.com/", nullptr, nullptr, 1); }

void Open_forum() { ::ShellExecute(nullptr, "open", "https://forum.uvnc.com/", nullptr, nullptr, 1); }

void Open_github() { ::ShellExecute(nullptr, "open", "https://github.com/ultravnc", nullptr, nullptr, 1); }

void Open_mastodon() { ::ShellExecute(nullptr, "open", "https://mastodon.social/@ultravnc", nullptr, nullptr, 1); }

void Open_bluesky() { ::ShellExecute(nullptr, "open", "https://bsky.app/profile/ultravnc.bsky.social", nullptr, nullptr, 1); }

void Open_facebook() { ::ShellExecute(nullptr, "open", "https://www.facebook.com/ultravnc1", nullptr, nullptr, 1); }

void Open_xtwitter() { ::ShellExecute(nullptr, "open", "https://x.com/ultravnc1", nullptr, nullptr, 1); }

void Open_reddit() { ::ShellExecute(nullptr, "open", "https://www.reddit.com/r/ultravnc", nullptr, nullptr, 1); }

void Open_openhub() { ::ShellExecute(nullptr, "open", "https://openhub.net/p/ultravnc", nullptr, nullptr, 1); }

#ifndef SC_20
namespace serviceHelpers
{
    void Set_stop_service_as_admin()
    {
        std::array<char, MAX_PATH> exe_file_name;
        ::GetModuleFileName(nullptr, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));

        SHELLEXECUTEINFO shExecInfo{};

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = winvncStopservice;
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
    }

    static bool IsValidServiceName(const char* const name)
    {
        for (const char* p = name; *p; ++p)
        {
            unsigned char c = static_cast<unsigned char>(*p);
            if (!(std::isalnum(c) || (c == ' ') || (c == '_') || (c == '-') || (c == '.')))
            {
                return false;
            }
        }
        return *name != '\0';
    }

    static void RunNetServiceCommand(const char* const verb)
    {
        if (!IsValidServiceName(UltraVNCService::service_name.data()))
        {
            return;
        }

        std::array<char, MAX_PATH> szSystemDir;
        if (::GetSystemDirectoryA(szSystemDir.data(), static_cast<UINT>(szSystemDir.size())) == 0)
        {
            return;
        }

        std::array<char, MAX_PATH + 64> command;
        ::_snprintf_s(command.data(), sizeof command, _TRUNCATE, "\"%s\\net.exe\" %s \"%s\"", szSystemDir.data(), verb, UltraVNCService::service_name.data());
        ::WinExec(command.data(), SW_HIDE);
    }

    void Real_stop_service() { RunNetServiceCommand("stop"); }

    void Set_start_service_as_admin()
    {
        std::array<char, MAX_PATH> exe_file_name;
        ::GetModuleFileName(nullptr, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));

        SHELLEXECUTEINFO shExecInfo{};

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = winvncStartservice;
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
    }

    void Real_start_service() { RunNetServiceCommand("start"); }

    void Set_install_service_as_admin()
    {
        std::array<char, MAX_PATH> exe_file_name;
        ::GetModuleFileName(nullptr, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));

        SHELLEXECUTEINFO shExecInfo{};

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = winvncInstallService;
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
    }

    void Set_uninstall_service_as_admin()
    {
        std::array<char, MAX_PATH> exe_file_name;
        ::GetModuleFileName(nullptr, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));

        SHELLEXECUTEINFO shExecInfo{};

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = winvncUnInstallService;
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
    }

    void winvncSecurityEditorHelper_as_admin()
    {
        std::array<char, MAX_PATH> exe_file_name;
        ::GetModuleFileName(nullptr, exe_file_name.data(), static_cast<DWORD>(exe_file_name.size()));

        SHELLEXECUTEINFO shExecInfo{};

        shExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        shExecInfo.fMask = 0;
        shExecInfo.hwnd = ::GetForegroundWindow();
        shExecInfo.lpVerb = "runas";
        shExecInfo.lpFile = exe_file_name.data();
        shExecInfo.lpParameters = winvncSecurityEditor;
        shExecInfo.lpDirectory = nullptr;
        shExecInfo.nShow = SW_SHOWNORMAL;
        shExecInfo.hInstApp = nullptr;
        ::ShellExecuteEx(&shExecInfo);
    }

    void make_upper(std::string& str)
    {
        // convert to uppercase
        std::transform(str.begin(), str.end(), str.begin(), toupper); //(int(*)(int))
    }

    //**************************************************************************
    // ExistServiceName() looks up service by application path. If found, the function
    // fills pszServiceName (must be at least 256+1 characters long).
    bool ExistServiceName(TCHAR* const pszAppPath, TCHAR* const pszServiceName)
    {
        // prepare given application path for matching against service list
        std::string appPath(pszAppPath);
        // convert to uppercase
        make_upper(appPath);

        // connect to serice control manager
        SC_HANDLE hManager = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
        if (!hManager)
        {
            return false;
        }

        DWORD dwBufferSize{ 0U };
        DWORD dwCount{ 0U };
        DWORD dwPosition{ 0U };
        bool bResult{ false };

        // call EnumServicesStatus() the first time to receive services array size
        BOOL bOK = ::EnumServicesStatus(hManager, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &dwBufferSize, &dwCount, &dwPosition);
        if (!bOK && (::GetLastError() == ERROR_MORE_DATA))
        {
            // allocate space per results from the first call
            ENUM_SERVICE_STATUS* pServices = reinterpret_cast<ENUM_SERVICE_STATUS*>(new UCHAR[dwBufferSize]);
            if (pServices)
            {
                // call EnumServicesStatus() the second time to actually get the services array
                bOK = ::EnumServicesStatus(hManager, SERVICE_WIN32, SERVICE_STATE_ALL, pServices, dwBufferSize, &dwBufferSize,
                                         &dwCount, &dwPosition);
                if (bOK)
                {
                    // iterate through all services returned by EnumServicesStatus()
                    for (DWORD i{ 0U }; (i < dwCount) && !bResult; ++i)
                    {
                        // open service
                        SC_HANDLE hService = ::OpenService(hManager, pServices[i].lpServiceName, GENERIC_READ);
                        if (!hService)
                        {
                            break;
                        }

                        // call QueryServiceConfig() the first time to receive buffer size
                        bOK = ::QueryServiceConfig(hService, nullptr, 0, &dwBufferSize);
                        if (!bOK && (::GetLastError() == ERROR_INSUFFICIENT_BUFFER))
                        {
                            // allocate space per results from the first call
                            QUERY_SERVICE_CONFIG* pServiceConfig = reinterpret_cast<QUERY_SERVICE_CONFIG*>(new UCHAR[dwBufferSize]);
                            if (pServiceConfig)
                            {
                                // call EnumServicesStatus() the second time to actually get service config
                                bOK = ::QueryServiceConfig(hService, pServiceConfig, dwBufferSize, &dwBufferSize);
                                if (bOK)
                                {
                                    // match given application name against executable path in the service config
                                    std::string servicePath(pServiceConfig->lpBinaryPathName);
                                    make_upper(servicePath);
                                    if (servicePath.find(appPath.c_str()) != -1)
                                    {
                                        bResult = true;
                                        ::strncpy_s(pszServiceName, 256, pServices[i].lpServiceName, 256);
                                        pszServiceName[255U] = 0;
                                    }
                                }

                                delete[] reinterpret_cast<UCHAR*>(pServiceConfig);
                            }
                        }

                        ::CloseServiceHandle(hService);
                    }
                }

                delete[] reinterpret_cast<UCHAR*>(pServices);
            }
        }

        // disconnect from serice control manager
        ::CloseServiceHandle(hManager);

        return bResult;
    }
}
#endif // SC_20

DWORD MessageBoxSecure(const HWND hWnd, const LPCTSTR lpText, const LPCTSTR lpCaption, UINT uType)
{
    DWORD retunvalue{ 0U };
    if (settings->RunningFromExternalService())
    {
        HDESK desktop = nullptr;
        HDESK old_desktop;
        desktop = ::OpenInputDesktop(0, FALSE,
                                   DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                       DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
        old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());
        if (desktop && old_desktop && (old_desktop != desktop))
        {
            ::SetThreadDesktop(desktop);
            if (uType & MB_SERVICE_NOTIFICATION)
            {
                retunvalue = static_cast<DWORD>(::MessageBox(hWnd, lpText, lpCaption, uType));
            }
            else if (uType & MB_YESNO)
            {
                BOOL bCheckboxChecked;
                retunvalue = helper::yesnoUVNCMessageBox(hInstResDLL, hWnd, const_cast<char*>(reinterpret_cast<const char *>(lpCaption)), const_cast<char*>(reinterpret_cast<const char *>(lpText)), "YES", "NO", "",
                                                         bCheckboxChecked);
            }
            else
            {
                if (uType & MB_OK)
                {
                    uType &= static_cast<unsigned long>(~MB_OK);
                }
                retunvalue = helper::yesUVNCMessageBox(hInstResDLL, hWnd, const_cast<char*>(reinterpret_cast<const char *>(lpText)), const_cast<char*>(reinterpret_cast<const char *>(lpCaption)), static_cast<int>(uType));
            }

            ::SetThreadDesktop(old_desktop);
            ::CloseDesktop(desktop);
        }
        else
        {
            retunvalue = 0;
        }
    }
    else
    {
        if (uType & MB_SERVICE_NOTIFICATION)
        {
            retunvalue = static_cast<DWORD>(::MessageBox(hWnd, lpText, lpCaption, uType));
        }
        else if (uType & MB_YESNO)
        {
            BOOL bCheckboxChecked;
            retunvalue =
                helper::yesnoUVNCMessageBox(hInstResDLL, hWnd, const_cast<char*>(reinterpret_cast<const char *>(lpCaption)), const_cast<char*>(reinterpret_cast<const char *>(lpText)), "YES", "NO", "", bCheckboxChecked);
        }
        else
        {
            if (uType & MB_OK)
            {
                uType &= static_cast<unsigned long>(~MB_OK);
            }
            retunvalue = helper::yesUVNCMessageBox(hInstResDLL, hWnd, const_cast<char*>(reinterpret_cast<const char *>(lpText)), const_cast<char*>(reinterpret_cast<const char *>(lpCaption)), static_cast<int>(uType));
        }
    }
    return retunvalue;
}

namespace desktopSelector
{
    int closeHandlesAndReturn(const HDESK threaddesktop, const HDESK inputdesktop, const int value)
    {
        if (threaddesktop)
        {
            ::CloseDesktop(threaddesktop);
        }
        if (inputdesktop)
        {
            ::CloseDesktop(inputdesktop);
        }
        return value;
    }
    int InputDesktopSelected()
    {
        // Get the input and thread desktops
        HDESK threaddesktop = ::GetThreadDesktop(::GetCurrentThreadId());
        HDESK inputdesktop = ::OpenInputDesktop(0, FALSE,
                                              DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                                  DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP);

        if (inputdesktop == nullptr)
        {
            if (!settings->RunningFromExternalService())
            {
                return closeHandlesAndReturn(threaddesktop, inputdesktop, 2);
            } //Running as SC we want to keep the viewer open in case UAC or screensaver jump in
            DWORD lasterror;
            lasterror = ::GetLastError();
            if (lasterror == 170)
            {
                return closeHandlesAndReturn(threaddesktop, inputdesktop, 1);
            }
            if (lasterror == 624)
            {
                return closeHandlesAndReturn(threaddesktop, inputdesktop, 1);
            }
            return closeHandlesAndReturn(threaddesktop, inputdesktop, 0);
        }

        DWORD dummy;
        std::array<char, 256U> threadname{};
        std::array<char, 256U> inputname{};

        if (!::GetUserObjectInformation(threaddesktop, UOI_NAME, &threadname, 256, &dummy))
        {
            if (!settings->RunningFromExternalService())
            {
                return closeHandlesAndReturn(threaddesktop, inputdesktop, 2);
            }
            return closeHandlesAndReturn(threaddesktop, inputdesktop, 0);
        }
        assert(dummy <= 256);
        if (!::GetUserObjectInformation(inputdesktop, UOI_NAME, &inputname, 256, &dummy))
        {
            if (!settings->RunningFromExternalService())
            {
                return closeHandlesAndReturn(threaddesktop, inputdesktop, 2);
            }
            return closeHandlesAndReturn(threaddesktop, inputdesktop, 0);
        }
        assert(dummy <= 256);

        if (std::strcmp(threadname.data(), inputname.data()) != 0)
        {
            if (!settings->RunningFromExternalService())
            {
                return closeHandlesAndReturn(threaddesktop, inputdesktop, 2);
            }
            return closeHandlesAndReturn(threaddesktop, inputdesktop, 0);
        }
        return closeHandlesAndReturn(threaddesktop, inputdesktop, 1);
    }

    BOOL SelectHDESK(const HDESK new_desktop)
    {
        HDESK old_desktop = ::GetThreadDesktop(::GetCurrentThreadId());

        DWORD dummy;
        std::array<char, 256U> new_name{};

        if (!::GetUserObjectInformation(new_desktop, UOI_NAME, &new_name, 256, &dummy))
        {
            vnclog.Print(LL_INTERR, VNCLOG("!GetUserObjectInformation \n"));
            return FALSE;
        }

        vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK to %s (%x) from %x\n"), new_name.data(), new_desktop, old_desktop);

        // Switch the desktop
        if (!::SetThreadDesktop(new_desktop))
        {
            vnclog.Print(LL_INTERR, VNCLOG("SelectHDESK:!SetThreadDesktop \n"));
            return FALSE;
        }
        return TRUE;
    }

    BOOL SelectDesktop(char* const name, HDESK* new_desktop)
    {
        HDESK desktop;
        vnclog.Print(LL_INTERR, VNCLOG("SelectDesktop \n"));
        if (name != nullptr)
        {
            vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop2 named\n"));
            // Attempt to open the named desktop
            desktop = ::OpenDesktop(name, 0, FALSE,
                                  DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                      DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
        }
        else
        {
            vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop2 NULL\n"));
            // No, so open the input desktop
            desktop = ::OpenInputDesktop(0, FALSE,
                                       DESKTOP_CREATEMENU | DESKTOP_CREATEWINDOW | DESKTOP_ENUMERATE | DESKTOP_HOOKCONTROL |
                                           DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS | DESKTOP_SWITCHDESKTOP | GENERIC_WRITE);
        }

        // Did we succeed?
        if (desktop == nullptr)
        {
            vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop2 \n"));
            return FALSE;
        }
        else
        {
            vnclog.Print(LL_INTERR, VNCLOG("OpenInputdesktop2 OK\n"));
        }

        // Switch to the new desktop
        if (!SelectHDESK(desktop))
        {
            // Failed to enter the new desktop, so free it!
            if (!::CloseDesktop(desktop))
            {
                vnclog.Print(LL_INTERR, VNCLOG("SelectDesktop failed to close desktop\n"));
            }
            return FALSE;
        }

        if (new_desktop)
        {
            if (*new_desktop)
            {
                ::CloseDesktop(*new_desktop);
            }
            *new_desktop = desktop;
        }

        // We successfully switched desktops!
        return TRUE;
    }
}

namespace postHelper
{
    UINT MENU_ADD_CLIENT_MSG = ::RegisterWindowMessage("WinVNC.AddClient.Message");
    UINT MENU_REPEATER_ID_MSG = ::RegisterWindowMessage("WinVNC.AddRepeaterID.Message");
    UINT MENU_AUTO_RECONNECT_MSG = ::RegisterWindowMessage("WinVNC.AddAutoClient.Message");
    UINT MENU_STOP_RECONNECT_MSG = ::RegisterWindowMessage("WinVNC.AddStopClient.Message");
    UINT MENU_STOP_ALL_RECONNECT_MSG = ::RegisterWindowMessage("WinVNC.AddStopAllClient.Message");
    UINT MENU_ADD_CLIENT_MSG_INIT = ::RegisterWindowMessage("WinVNC.AddClient.Message.Init");
    UINT MENU_ADD_CLIENT6_MSG_INIT = ::RegisterWindowMessage("WinVNC.AddClient6.Message.Init");
    UINT MENU_ADD_CLIENT6_MSG = ::RegisterWindowMessage("WinVNC.AddClient6.Message");
    UINT MENU_TRAYICON_BALLOON_MSG = ::RegisterWindowMessage("WinVNC.TrayIconBalloon2.Message");
    UINT FileTransferSendPacketMessage = ::RegisterWindowMessage("UltraVNC.Viewer.FileTransferSendPacketMessage");

    in6_addr G_LPARAM_IN6{};

    BOOL PostAddAutoConnectClient(const char* const pszId)
    {
        ATOM aId = INVALID_ATOM;
        if (pszId)
        {
            aId = ::GlobalAddAtom(pszId);
        }
        return (PostToWinVNC(MENU_AUTO_RECONNECT_MSG, 0, static_cast<LPARAM>(aId)));
    }

    BOOL PostAddNewRepeaterClient()
    {
        // assumes the -repeater command line set the repeater global variable.
        // Post to the UltraVNC Server menu window (usually expected to fail at program startup)
        if (!PostToWinVNC(MENU_ADD_CLIENT_MSG, static_cast<WPARAM>(0xFFFFFFFF), static_cast<LPARAM>(0xFFFFFFFF)))
        {
            return FALSE;
        }
        return TRUE;
    }

    BOOL PostAddStopConnectClient() { return (PostToWinVNC(MENU_STOP_RECONNECT_MSG, 0, 0)); }

    BOOL PostAddStopConnectClientAll()
    {
        PostToWinVNC(MENU_STOP_RECONNECT_MSG, 0, 0);              // stop running reconnect in server class
        return (PostToWinVNC(MENU_STOP_ALL_RECONNECT_MSG, 0, 0)); // disable reconnect for tunning clients
    }

    BOOL PostAddNewClientInit(unsigned long ipaddress, const unsigned short port)
    {
        // Post to the UltraVNC Server menu window
        if (!PostToWinVNC(MENU_ADD_CLIENT_MSG_INIT, static_cast<WPARAM>(port), static_cast<LPARAM>(ipaddress)))
        {
            vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient failed\n"));
            if ((port == 1111) && (ipaddress == 1111))
            {
                ClientTimerReconnect = true;
            }
            return FALSE;
        }

        return TRUE;
    }

    BOOL PostAddNewClient4(unsigned long ipaddress, const unsigned short port)
    {
        // Post to the UltraVNC Server menu window
        if (!PostToWinVNC(MENU_ADD_CLIENT_MSG, static_cast<WPARAM>(port), static_cast<LPARAM>(ipaddress)))
        {
            vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient failed\n"));
            if ((port == 1111) && (ipaddress == 1111))
            {
                ClientTimerReconnect = true;
            }
            return FALSE;
        }

        return TRUE;
    }

    BOOL PostAddNewClientInit4(unsigned long ipaddress, const unsigned short port)
    {
        // Post to the UltraVNC Server menu window
        if (!PostToWinVNC(MENU_ADD_CLIENT_MSG_INIT, static_cast<WPARAM>(port), static_cast<LPARAM>(ipaddress)))
        {
            vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient failed\n"));
            if ((port == 1111) && (ipaddress == 1111))
            {
                ClientTimerReconnect = true;
            }
            return FALSE;
        }
        return TRUE;
    }
    BOOL PostAddNewClient6(in6_addr* const ipaddress, const unsigned short port)
    {
        // Post to the UltraVNC Server menu window
        // We can not sen a IPv6 address with a LPARAM, so we fake the message by copying in a gloobal var.
        std::memcpy(&G_LPARAM_IN6, ipaddress, sizeof(in6_addr));
        if (!PostToWinVNC(MENU_ADD_CLIENT6_MSG, static_cast<WPARAM>(port), reinterpret_cast<LPARAM>(ipaddress)))
        {
            return FALSE;
        }
        return TRUE;
    }

    BOOL PostAddNewClientInit6(in6_addr* const ipaddress, const unsigned short port)
    {
        // Post to the UltraVNC Server menu window
        // We can not sen a IPv6 address with a LPARAM, so we fake the message by copying in a gloobal var.
        std::memcpy(&G_LPARAM_IN6, ipaddress, sizeof(in6_addr));
        if (!PostToWinVNC(MENU_ADD_CLIENT6_MSG_INIT, static_cast<WPARAM>(port), reinterpret_cast<LPARAM>(ipaddress)))
        {
            return FALSE;
        }
        return TRUE;
    }

    BOOL PostAddNewClient(unsigned long ipaddress, const unsigned short port)
    {
        // Post to the UltraVNC Server menu window
        if (!PostToWinVNC(MENU_ADD_CLIENT_MSG, static_cast<WPARAM>(port), static_cast<LPARAM>(ipaddress)))
        {
            vnclog.Print(LL_INTERR, VNCLOG("PostAddNewClient failed\n"));
            if ((port == 1111) && (ipaddress == 1111))
            {
                ClientTimerReconnect = true;
            }
            return FALSE;
        }
        return TRUE;
    }

    BOOL PostAddConnectClient(const char* const pszId)
    {
        ATOM aId = INVALID_ATOM;
        if (pszId)
        {
            aId = ::GlobalAddAtom(pszId);
        }
        return (PostToWinVNC(MENU_REPEATER_ID_MSG, 0, static_cast<LPARAM>(aId)));
    }

    BOOL PostToThisWinVNC(const UINT message, const WPARAM wParam, const LPARAM lParam)
    {
        //adzm 2010-02-10 - Finds the appropriate VNC window for this process
        HWND hservwnd = FindWinVNCWindow(true);
        if (hservwnd == nullptr)
        {
            return FALSE;
        }

        // Post the message to UltraVNC Server
        ::PostMessage(hservwnd, message, wParam, lParam);
        return TRUE;
    }

    BOOL PostToWinVNC(const UINT message, const WPARAM wParam, const LPARAM lParam)
    {
        // Locate the hidden UltraVNC Server menu window
        // adzm 2010-02-10 - If we are in SC mode, then we know we want to only post messages to our own instance. This prevents
        // conflicts if the user already has another copy of a UltraVNC Server derived application running.
        if (allowMultipleInstances || settings->getScExit() || settings->getScPrompt())
        {
            return PostToThisWinVNC(message, wParam, lParam);
        }

        //adzm 2010-02-10 - Finds the appropriate VNC window
        HWND hservwnd = FindWinVNCWindow(false);
        if (hservwnd == nullptr)
        {
            return FALSE;
        }

        // Post the message to UltraVNC Server
        ::PostMessage(hservwnd, message, wParam, lParam);
        return TRUE;
    }

    HWND FindWinVNCWindow(const bool bThisProcess)
    {
        // Locate the hidden UltraVNC Server menu window
        if (!bThisProcess)
        {
            // Find any window with the MENU_CLASS_NAME window class
            HWND returnvalue = ::FindWindow(MENU_CLASS_NAME, nullptr);
            if (returnvalue == nullptr)
            {
                goto nullreturn;
            }
            return returnvalue;
        }
        else
        {
            // Find one that matches the class and is the same process
            HWND hwndZ = nullptr;
            HWND hwndServer = nullptr;
            while (!hwndServer)
            {
                hwndServer = ::FindWindowEx(nullptr, hwndZ, MENU_CLASS_NAME, nullptr);

                if (hwndServer != nullptr)
                {
                    DWORD dwProcessId{ 0U };
                    ::GetWindowThreadProcessId(hwndServer, &dwProcessId);

                    if (dwProcessId == ::GetCurrentProcessId())
                    {
                        return hwndServer;
                    }
                    else
                    {
                        hwndZ = hwndServer;
                        hwndServer = nullptr;
                    }
                }
                else
                {
                    goto nullreturn;
                }
            }
        }

    nullreturn:
        return G_MENU_HWND;
    }
}

namespace processHelper
{
    DWORD GetExplorerLogonPid()
    {
        std::array<char, 129U> alternate_shell;
        ::strcpy_s(alternate_shell.data(), alternate_shell.size(), "");
        ::strcpy_s(alternate_shell.data(), alternate_shell.size(), settings->getAlternateShell());
        DWORD dwSessionId;
        DWORD dwExplorerLogonPid{ 0U };
        PROCESSENTRY32 procEntry{};
        dwSessionId = ::WTSGetActiveConsoleSessionId();
        if (GetSystemMetrics(SM_REMOTESESSION))
        {
            DWORD dw = ::GetCurrentProcessId();
            DWORD pSessionId{ 0xFFFFFFFFU };
            ::ProcessIdToSessionId(dw, &pSessionId);
            dwSessionId = pSessionId;
        }

        HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap == INVALID_HANDLE_VALUE)
        {
            return 0;
        }
        procEntry.dwSize = sizeof(PROCESSENTRY32);
        if (!::Process32First(hSnap, &procEntry))
        {
            ::CloseHandle(hSnap);
            return 0;
        }

        do
        {
            if ((::_stricmp(procEntry.szExeFile, "explorer.exe") == 0) ||
                ((std::strlen(alternate_shell.data()) != 0) && (::_stricmp(procEntry.szExeFile, alternate_shell.data()) == 0)))
            {
                DWORD dwExplorerSessId{ 0U };
                if (::ProcessIdToSessionId(procEntry.th32ProcessID, &dwExplorerSessId) && (dwExplorerSessId == dwSessionId))
                {
                    dwExplorerLogonPid = procEntry.th32ProcessID;
                    break;
                }
            }
        } while (::Process32Next(hSnap, &procEntry));
        ::CloseHandle(hSnap);
        return dwExplorerLogonPid;
    }

    bool GetConsoleUser(char* const buffer, UINT size)
    {
        HANDLE hPToken = DesktopUsersToken::getInstance()->getDesktopUsersToken();
        if (hPToken == nullptr)
        {
            ::strcpy_s(buffer, UNLEN + 1, "");
            return 0;
        }

        std::array<char, 16384U> aa{};
        // token user
        TOKEN_USER* ptu;
        DWORD needed;
        ptu = reinterpret_cast<TOKEN_USER*>(aa.data()); //malloc( 16384 );
        if (::GetTokenInformation(hPToken, TokenUser, ptu, 16384, &needed))
        {
            std::array<char, 64U> DomainName;
            std::memset(DomainName.data(), 0, sizeof(DomainName));
            DWORD DomainSize;
            DomainSize = sizeof(DomainName) - 1;
            SID_NAME_USE SidType;
            DWORD dwsize = size;
            ::LookupAccountSid(nullptr, ptu->User.Sid, buffer, &dwsize, DomainName.data(), &DomainSize, &SidType);
            //free(ptu);
            return 1;
        }
        //free(ptu);
        ::strcpy_s(buffer, UNLEN + 1, "");
        return 0;
    }

    BOOL GetCurrentUser(char* const buffer, const UINT size) // RealVNC 336 change
    {
        BOOL g_impersonating_user = FALSE;
        // How to obtain the name of the current user depends upon the OS being used
        if (settings->RunningFromExternalService())
        {
            // Get the current Window station
            g_impersonating_user = TRUE;

            HWINSTA station = ::GetProcessWindowStation();
            if (station == nullptr)
            {
                return FALSE;
            }

            DWORD usersize;
            ::GetUserObjectInformation(station, UOI_USER_SID, nullptr, 0, &usersize);

            // Check the required buffer size isn't zero
            if (usersize == 0)
            {
                // No user is logged in - ensure we're not impersonating anyone
                ::RevertToSelf();
                g_impersonating_user = FALSE;
                if (std::strlen("") >= size)
                {
                    return FALSE;
                }
                ::strcpy_s(buffer, UNLEN + 1, "");
                return TRUE;
            }

            if (!g_impersonating_user)
            {
                if (std::strlen("") >= size)
                {
                    return FALSE;
                }
                ::strcpy_s(buffer, UNLEN + 1, "");
                return TRUE;
            }
        }

        DWORD length = size;
        if (GetConsoleUser(buffer, size) == 0)
        {
            if (::GetUserName(buffer, &length) == 0)
            {
                UINT error = ::GetLastError();
                if (error == ERROR_NOT_LOGGED_ON)
                {
                    // No user logged on
                    if (std::strlen("") >= size)
                    {
                        return FALSE;
                    }
                    ::strcpy_s(buffer, UNLEN + 1, "");
                    return TRUE;
                }
                else
                {
                    // Genuine error...
                    vnclog.Print(LL_INTERR, VNCLOG("GetUsername error %d\n"), ::GetLastError());
                    return FALSE;
                }
            }
        }
        return TRUE;
    }

    BOOL CurrentUser(char* const buffer, const UINT size)
    {
        // Attempt to get the current user
        BOOL result = GetCurrentUser(buffer, size);

        // If no user is logged in, and we're not running as an external service
        if (result && (std::strcmp(buffer, "") == 0) && !settings->RunningFromExternalService())
        {
            // Ensure buffer has enough space for "Default" + null terminator
            if (size >= sizeof("Default"))
            {
                ::strncpy_s(buffer, size, "Default", _TRUNCATE); // Set buffer to "Default" safely
            }
        }

        return result;
    }
#ifndef SC_20
    bool IsServiceRunning()
    {
        bool isRunning{ false };

        // Open the Service Control Manager
        SC_HANDLE hSCM = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!hSCM)
        {
            return false;
        }

        // Open the service
        SC_HANDLE hService = ::OpenService(hSCM, UltraVNCService::service_name.data(), SERVICE_QUERY_STATUS);
        if (!hService)
        {
            ::CloseServiceHandle(hSCM);
            return false;
        }

        // Query the service status
        SERVICE_STATUS_PROCESS serviceStatus;
        DWORD bytesNeeded;
        if (::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&serviceStatus), sizeof(SERVICE_STATUS_PROCESS),
                                 &bytesNeeded))
        {
            if (serviceStatus.dwCurrentState == SERVICE_RUNNING)
            {
                isRunning = true;
            }
        }

        // Cleanup
        ::CloseServiceHandle(hService);
        ::CloseServiceHandle(hSCM);

        return isRunning;
    }
#endif
    bool IsServiceInstalled()
    {
        bool serviceInstalled{ false };

#ifndef SC_20
        // Open the Service Control Manager with permission to enumerate services
        SC_HANDLE hSCM = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
        if (hSCM)
        {
            // Attempt to open the specified service with query configuration access
            SC_HANDLE hService = ::OpenService(hSCM, UltraVNCService::service_name.data(), SERVICE_QUERY_CONFIG);
            if (hService)
            {
                serviceInstalled = true;
                ::CloseServiceHandle(hService);
            }
            ::CloseServiceHandle(hSCM);
        }
#endif // SC_20

        return serviceInstalled;
    }

    DWORD GetCurrentConsoleSessionID() { return ::WTSGetActiveConsoleSessionId(); }

    BOOL IsWSLocked()
    {
        bool bLocked{ false };

        // Original code does not work if running as a service... apparently no access to the desktop.
        // Alternative is to check for a running LogonUI.exe (if present, system is either not logged in or locked)
        // Only check LogonUI.exe in the same session as the current process to avoid false positives from:
        // - other RDP sessions showing their login screen
        // - Hyper-V Enhanced Session which keeps LogonUI.exe alive in the VM session
        DWORD mySessionId{ 0U };
        ::ProcessIdToSessionId(::GetCurrentProcessId(), &mySessionId);

        HANDLE hSnap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

        PROCESSENTRY32W procentry{};
        procentry.dwSize = sizeof(procentry);

        if (::Process32FirstW(hSnap, &procentry))
        {
            do
            {
                if (!::_wcsicmp(procentry.szExeFile, L"LogonUI.exe"))
                {
                    DWORD loguiSessionId{ 0U };
                    if (::ProcessIdToSessionId(procentry.th32ProcessID, &loguiSessionId) && (loguiSessionId == mySessionId))
                    {
                        bLocked = true;
                        break;
                    }
                }
            } while (::Process32NextW(hSnap, &procentry));
        }
        ::CloseHandle(hSnap);
        return bLocked;
    }
}
