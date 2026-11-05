// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "stdafx.h"

#include "resource.h"

#include <array>
#include <cstring>
#include <time.h>

#if !defined(__MINGW32__)
#include "upnp.h"

#endif
#include "firewall.h"
#include "log.h"

#include <iphlpapi.h>

#pragma comment(lib, "iphlpapi")
#include <shlwapi.h>

#pragma comment(lib, "shlwapi")
#ifndef _WINSOCK2API_
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")
#endif
#include <stdio.h>

static std::array<char, MAX_PATH> service_path;
static std::array<char, MAX_PATH> service_file;
std::array<char, 256U> service_name{"uvnc_service"};
char* app_name = "UltraVNC";
#define VNCDEPENDENCIES "Tcpip\0\0"


////////////////////////////////////////////////////////////////////////////////
void set_service_description()
{
    // Add service description
    DWORD dw;
    HKEY hKey;
    std::array<char, 256U> tempName;
    char desc[] = "UltraVNC Server provides secure remote desktop sharing";
    _snprintf(tempName.data(), sizeof tempName, "SYSTEM\\CurrentControlSet\\Services\\%s", service_name.data());
    ::RegCreateKeyEx(HKEY_LOCAL_MACHINE, tempName.data(), 0, REG_NONE, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, nullptr, &hKey,
                   &dw);
    ::RegSetValueEx(hKey, "Description", 0, REG_SZ, (const BYTE*)desc, static_cast<DWORD>((std::strlen(desc) + 1)));


    ::RegCloseKey(hKey);
}
////////////////////////////////////////////////////////////////////////////////
static int pad()
{
    std::array<char, MAX_PATH> exe_file_name;
    std::array<char, MAX_PATH> dir;
    char* ptr;
    ::GetModuleFileName(0, exe_file_name.data(), exe_file_name.size());

    /* set current directory */
    ::strcpy_s(dir.data(), dir.size(), exe_file_name.data());
    ptr = std::strrchr(dir.data(), '\\'); /* last backslash */
    if (ptr)
    {
        ptr[1U] = '\0';
    } /* truncate program name */
    if (!::SetCurrentDirectory(dir.data()))
    {
        return 1;
    }

    std::strcpy(service_path.data(), "\"");
    std::strcat(service_path.data(), dir.data());
    std::strcat(service_path.data(), "\\winvnc.exe");
    std::strcat(service_path.data(), "\" -service");

    std::strcpy(service_file.data(), dir.data());
    std::strcat(service_file.data(), "\\winvnc.exe");
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
DWORD MessageBoxSecure(const HWND hWnd, const LPCTSTR lpText, const LPCTSTR lpCaption, const UINT uType)
{
    DWORD retunvalue;
    retunvalue = ::MessageBox(hWnd, lpText, lpCaption, uType);
    return retunvalue;
}
////////////////////////////////////////////////////////////////////////////////
bool existFile(char* const mychar)
{
    DWORD value1 = ::GetFileAttributes(mychar);
    if (value1 == INVALID_FILE_ATTRIBUTES)
    {
        DWORD errnr = ::GetLastError();
        return false;
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////
int install_service(void)
{

    SC_HANDLE scm, service;
    pad();
    if (!existFile(service_file.data()))
    {
        MessageBoxSecure(nullptr,
                         "Verify that winvnc.exe and uvnc_settings are in the same folder\n can not create the uvnc_service",
                         app_name, MB_ICONERROR);
        return 1;
    }
    scm = ::OpenSCManager(0, 0, SC_MANAGER_CREATE_SERVICE);
    if (!scm)
    {
        MessageBoxSecure(nullptr, "Failed to open service control manager", app_name, MB_ICONERROR);
        return 1;
    }
    //"UltraVNC Server provides secure remote desktop sharing"
    service = ::CreateService(scm, service_name.data(), service_name.data(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                            SERVICE_ERROR_NORMAL, service_path.data(), nullptr, nullptr, VNCDEPENDENCIES, nullptr, nullptr);
    if (!service)
    {
        DWORD myerror = ::GetLastError();
        if (myerror == ERROR_ACCESS_DENIED)
        {
            MessageBoxSecure(nullptr, "Failed: Permission denied", app_name, MB_ICONERROR);
            ::CloseServiceHandle(scm);
            return 1;
        }
        if (myerror == ERROR_SERVICE_EXISTS)
        {
            //MessageBoxSecure(NULL, "Failed: Already exist",
            //"UltraVNC", MB_ICONERROR);
            ::CloseServiceHandle(scm);
            return 1;
        }

        MessageBoxSecure(nullptr, "Failed to create a new service", app_name, MB_ICONERROR);
        ::CloseServiceHandle(scm);
        return 1;
    }
    else
    {
        set_service_description();
    }
    ::CloseServiceHandle(service);
    ::CloseServiceHandle(scm);
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
int uninstall_service(void)
{
    SC_HANDLE scm, service;
    SERVICE_STATUS serviceStatus;

    scm = ::OpenSCManager(0, 0, SC_MANAGER_CONNECT);
    if (!scm)
    {
        MessageBoxSecure(nullptr, "Failed to open service control manager", app_name, MB_ICONERROR);
        return 1;
    }

    service = ::OpenService(scm, service_name.data(), SERVICE_QUERY_STATUS | DELETE);
    if (!service)
    {
        DWORD myerror = ::GetLastError();
        if (myerror == ERROR_ACCESS_DENIED)
        {
            MessageBoxSecure(nullptr, "Failed: Permission denied", app_name, MB_ICONERROR);
            ::CloseServiceHandle(scm);
            return 1;
        }
        if (myerror == ERROR_SERVICE_DOES_NOT_EXIST)
        {
#if 0
			MessageBoxSecure(nullptr, "Failed: Service is not installed",
            app_name, MB_ICONERROR);
#endif
            ::CloseServiceHandle(scm);
            return 1;
        }

        MessageBoxSecure(nullptr, "Failed to open the service", app_name, MB_ICONERROR);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (!::QueryServiceStatus(service, &serviceStatus))
    {
        MessageBoxSecure(nullptr, "Failed to query service status", app_name, MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    if (serviceStatus.dwCurrentState != SERVICE_STOPPED)
    {
        //MessageBoxSecure(NULL, "The service is still running, disable it first",
        //    "UltraVNC", MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        ::Sleep(2500);
        uninstall_service();
        return 1;
    }
    if (!::DeleteService(service))
    {
        MessageBoxSecure(nullptr, "Failed to delete the service", app_name, MB_ICONERROR);
        ::CloseServiceHandle(service);
        ::CloseServiceHandle(scm);
        return 1;
    }
    ::CloseServiceHandle(service);
    ::CloseServiceHandle(scm);
    return 0;
}
////////////////////////////////////////////////////////////////////////////////
bool s_installed{ 0 };
bool s_running{ 0 };

void Real_start_service()
{
    std::array<char, MAX_PATH + 32> command; // 29 January 2008 jdp
    _snprintf(command.data(), sizeof command, "net start \"%s\"", service_name.data());
    ::WinExec(command.data(), SW_HIDE);
}

void Real_stop_service()
{
    std::array<char, MAX_PATH + 32> command; // 29 January 2008 jdp
    _snprintf(command.data(), sizeof command, "net stop \"%s\"", service_name.data());
    ::WinExec(command.data(), SW_HIDE);
}

bool IsInstalled()
{
    s_running = false;

    BOOL bResult = FALSE;
    SC_HANDLE hSCM = ::OpenSCManager(nullptr,                       // local machine
                                     nullptr,                       // ServicesActive database
                                     SC_MANAGER_ENUMERATE_SERVICE); // full access
    if (hSCM)
    {
        SC_HANDLE hService = ::OpenService(hSCM, service_name.data(), SERVICE_ALL_ACCESS);
        if (hService)
        {
            bResult = TRUE;
            SERVICE_STATUS_PROCESS ssp;
            DWORD dwBytesNeeded;
            if (::QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS),
                                     &dwBytesNeeded) != 0)
            {
                if (ssp.dwCurrentState == SERVICE_RUNNING)
                {
                    s_running = true;
                }
            }
            else
            {
                DWORD aa = ::GetLastError();
                ++aa;
            }

            ::CloseServiceHandle(hService);
        }
        ::CloseServiceHandle(hSCM);
    }
    return (FALSE != bResult);
}


BOOL CALLBACK DlgProcService(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        ::SetTimer(hwnd, 1001, 5000, nullptr);
        pad();
        s_installed = IsInstalled();
        if (!s_installed)
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_INSTALSS), TRUE);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_INSTALSS), FALSE);
        }
        if (s_installed)
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_UNINSTALLS), TRUE);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_UNINSTALLS), FALSE);
        }
        if (!s_running && s_installed)
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_STARTS), TRUE);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_STARTS), FALSE);
        }
        if (s_running)
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_STOPS), TRUE);
        }
        else
        {
            ::EnableWindow(::GetDlgItem(hwnd, IDC_STOPS), FALSE);
        }
        return TRUE;
    }

    case WM_TIMER:
        if (wParam == 1001)
        {
            s_installed = IsInstalled();
            if (!s_installed)
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_INSTALSS), TRUE);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_INSTALSS), FALSE);
            }
            if (s_installed)
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_UNINSTALLS), TRUE);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_UNINSTALLS), FALSE);
            }
            if (!s_running && s_installed)
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_STARTS), TRUE);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_STARTS), FALSE);
            }
            if (s_running)
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_STOPS), TRUE);
            }
            else
            {
                ::EnableWindow(::GetDlgItem(hwnd, IDC_STOPS), FALSE);
            }
            break;
        }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_HELP2:
            if (lParam == 6)

            {
                std::array<char, 256U> link;
                std::strcpy(link.data(), "https://uvnc.com/webhelp/");
                std::strcat(link.data(), "service");
                std::strcat(link.data(), ".html");
                ::ShellExecute(::GetDesktopWindow(), "open", link.data(), "", 0, SW_SHOWNORMAL);
            }
            break;
        case IDOK:
            break;
        case IDCANCEL:
            ::EndDialog(hwnd, IDCANCEL);
            return TRUE;
        case IDC_INSTALSS:
            install_service();
            break;
        case IDC_UNINSTALLS:
            Real_stop_service();
            ::Sleep(2000);
            uninstall_service();
            break;
        case IDC_STARTS:
            Real_start_service();
            break;
        case IDC_STOPS:
            Real_stop_service();
            break;
        }
        return 0;
    }

    return 0;
}