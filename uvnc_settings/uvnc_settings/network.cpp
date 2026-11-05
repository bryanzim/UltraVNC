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

#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")
#endif

extern LONG SocketConnect;
extern LONG HTTPConnect;
extern LONG AutoPortSelect;
extern LONG PortNumber;
extern LONG HttpPortNumber;
extern LONG LoopbackOnly;
extern LONG AllowLoopback;

extern bool FireWall_status;
extern bool Service_status;
bool UPNP_status{ false };
bool UPNP_status_checked{ false };

HWND EditControl;
HWND networkproc;
std::array<char, 256U> strLocalIP;
std::array<char, 256U> strExternIP;

DWORD WINAPI upnpthread(const LPVOID lpParam)
{
#if !defined(__MINGW32__)
    if (FAILED(::CoInitializeEx(0, COINIT_APARTMENTTHREADED)))
    {
        ::CoUninitialize();
        return 0;
    }
    WSADATA wdata;
    ::WSAStartup(MAKEWORD(2, 2), &wdata);

    UPnP UPnPvar;

    PIP_ADAPTER_INFO pAdapterInfo = nullptr;
    DWORD dwRetVal{ 0U };

    pAdapterInfo = (IP_ADAPTER_INFO*)std::malloc(sizeof(IP_ADAPTER_INFO));
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable

    if (::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
    {
        std::free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)std::malloc(ulOutBufLen);
    }

    if ((dwRetVal = ::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
    {
        std::strcpy(strLocalIP.data(), pAdapterInfo->IpAddressList.IpAddress.String);
        debug("Using port 5999 for Ext IP");
        UPnPvar.Set_UPnP(strLocalIP.data(), "TCP", "UVNC", 5999);
        UPnPvar.OpenPorts(false);
        std::strcpy(strExternIP.data(), UPnPvar.GetExternalIP());
        ::SendMessage(networkproc, WM_COMMAND, 2003, 0);
    }
    std::free(pAdapterInfo);
    UPnPvar.ClosePorts(false);
    debug("Close 5999");
    ::WSACleanup();
    ::CoUninitialize();
#endif //__MINGW32__
    return 0;
}

DWORD WINAPI checkthread(const LPVOID lpParam)
{
#if !defined(__MINGW32__)
    if (FAILED(::CoInitializeEx(0, COINIT_APARTMENTTHREADED)))
    {
        ::CoUninitialize();
        return 0;
    }
    WSADATA wdata;
    ::WSAStartup(MAKEWORD(2, 2), &wdata);
    FirewallCheck(networkproc);

    ::WSACleanup();
    ::CoUninitialize();
#endif //__MINGW32__
    return 0;
}

DWORD WINAPI mapthread(const LPVOID lpParam)
{
#if !defined(__MINGW32__)
    if (FAILED(::CoInitializeEx(0, COINIT_APARTMENTTHREADED)))
    {
        ::CoUninitialize();
        return 0;
    }
    WSADATA wdata;
    ::WSAStartup(MAKEWORD(2, 2), &wdata);
    checksetport(PortNumber);

    UPnP UPnPvar;

    PIP_ADAPTER_INFO pAdapterInfo = nullptr;
    DWORD dwRetVal{ 0U };

    pAdapterInfo = (IP_ADAPTER_INFO*)std::malloc(sizeof(IP_ADAPTER_INFO));
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable

    if (::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
    {
        std::free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)std::malloc(ulOutBufLen);
    }

    if ((dwRetVal = ::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
    {
        std::strcpy(strLocalIP.data(), pAdapterInfo->IpAddressList.IpAddress.String);
        UPnPvar.Set_UPnP(strLocalIP.data(), "TCP", "UVNC_TCP", (const short)PortNumber);
        UPnPvar.OpenPorts(true);
        std::strcpy(strExternIP.data(), UPnPvar.GetExternalIP());
        ::SendMessage(networkproc, WM_COMMAND, 2003, 0);
    }
    std::free(pAdapterInfo);
    ::WSACleanup();
    ::CoUninitialize();
#endif //__MINGW32__
    return 0;
}

DWORD WINAPI unmapthread(const LPVOID lpParam)
{
#if !defined(__MINGW32__)
    if (FAILED(::CoInitializeEx(0, COINIT_APARTMENTTHREADED)))
    {
        ::CoUninitialize();
        return 0;
    }
    WSADATA wdata;
    ::WSAStartup(MAKEWORD(2, 2), &wdata);

    UPnP UPnPvar;

    PIP_ADAPTER_INFO pAdapterInfo = nullptr;
    DWORD dwRetVal{ 0U };

    pAdapterInfo = (IP_ADAPTER_INFO*)std::malloc(sizeof(IP_ADAPTER_INFO));
    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);

    // Make an initial call to GetAdaptersInfo to get
    // the necessary size into the ulOutBufLen variable

    if (::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen) == ERROR_BUFFER_OVERFLOW)
    {
        std::free(pAdapterInfo);
        pAdapterInfo = (IP_ADAPTER_INFO*)std::malloc(ulOutBufLen);
    }

    if ((dwRetVal = ::GetAdaptersInfo(pAdapterInfo, &ulOutBufLen)) == NO_ERROR)
    {
        std::strcpy(strLocalIP.data(), pAdapterInfo->IpAddressList.IpAddress.String);
        UPnPvar.Set_UPnP(strLocalIP.data(), "TCP", "UVNC_TCP", (const short)PortNumber);
        //UPnPvar.OpenPorts(true);
        std::strcpy(strExternIP.data(), UPnPvar.GetExternalIP());
        ::SendMessage(networkproc, WM_COMMAND, 2003, 0);
    }
    std::free(pAdapterInfo);
    UPnPvar.ClosePorts(true);
    ::WSACleanup();
    ::CoUninitialize();
#endif //__MINGW32__
    return 0;
}

DWORD WINAPI Fixhread(const LPVOID lpParam)
{
#if !defined(__MINGW32__)
    if (FAILED(::CoInitializeEx(0, COINIT_APARTMENTTHREADED)))
    {
        ::CoUninitialize();
        return 0;
    }
    WSADATA wdata;
    ::WSAStartup(MAKEWORD(2, 2), &wdata);
    if (!Service_status)
    {
        if (ControlSSDPService(true))
        {
            Service_status = true;
        }
    }
    if (!FireWall_status)
    {
        if (IsICSConnEnabled())
        {
            debug("WARNING!  Internet Connection Sharing is active. Opening UPnP ports could expose the computer directly to the "
                  "Internet.");
        }
        if (ControlUPnPPorts(true))
        {
            FireWall_status = true;
        }
    }

    ::WSACleanup();
    ::CoUninitialize();
    ::SendMessage(networkproc, WM_COMMAND, 2001, 0);
#endif //__MINGW32__
    return 0;
}
bool initdone{ false };
BOOL CALLBACK DlgProcOptions1(const HWND hwnd, const UINT uMsg, const WPARAM wParam, const LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG: {
        initdone = false;
        ::SendMessage(::GetDlgItem(hwnd, IDC_CONNECT_SOCK), BM_SETCHECK, SocketConnect, 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_CONNECT_HTTP), BM_SETCHECK, HTTPConnect, 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_ALLOWLOOPBACK), BM_SETCHECK, AllowLoopback, 0);
        ::SendMessage(::GetDlgItem(hwnd, IDC_LOOPBACKONLY), BM_SETCHECK, LoopbackOnly, 0);
        ::SetDlgItemInt(hwnd, IDC_PORTRFB, PortNumber, FALSE);
        ::SetDlgItemInt(hwnd, IDC_PORTHTTP, HttpPortNumber, FALSE);
        ::CheckDlgButton(hwnd, IDC_PORTNO_AUTO, (AutoPortSelect) ? BST_CHECKED : BST_UNCHECKED);
        ::CheckDlgButton(hwnd, IDC_SPECPORT, (!AutoPortSelect) ? BST_CHECKED : BST_UNCHECKED);
        EditControl = ::GetDlgItem(hwnd, IDC_EDIT3);
        networkproc = hwnd;
        initdone = true;
        ::SendMessage(hwnd, WM_COMMAND, 2005, 0);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case 2001:
            if (FireWall_status && Service_status)
            {
                UPNP_status = true;
            }
            else
            {
                UPNP_status = false;
            }
            UPNP_status_checked = true;
            ::SendMessage(hwnd, WM_COMMAND, 2002, 0);
            break;
        case 2003:
            ::SetWindowText(::GetDlgItem(hwnd, IDC_LOCAL), strLocalIP.data());
            ::SetWindowText(::GetDlgItem(hwnd, IDC_REMOTE), strExternIP.data());
            break;
        case IDC_CHECKUPNP: {
            DWORD dw;
            HANDLE thread = ::CreateThread(nullptr, 0, checkthread, nullptr, 0, &dw);
        }
        break;
        case IDC_TESTNET: {
            DWORD dw;
            HANDLE thread = ::CreateThread(nullptr, 0, upnpthread, nullptr, 0, &dw);
        }
        break;
        case IDC_FIXUPNP: {
            DWORD dw;
            HANDLE thread = ::CreateThread(nullptr, 0, Fixhread, nullptr, 0, &dw);
        }
        break;
        case IDC_UPNPON: {
            DWORD dw;
            HANDLE thread = ::CreateThread(nullptr, 0, mapthread, nullptr, 0, &dw);
        }
        break;
        case IDC_UPNPOFF: {
            DWORD dw;
            HANDLE thread = ::CreateThread(nullptr, 0, unmapthread, nullptr, 0, &dw);
        }
        break;
        case IDC_HELP2:
            if (lParam == 0)

            {
                std::array<char, 256U> link;
                std::strcpy(link.data(), "https://uvnc.com/webhelp/");
                std::strcat(link.data(), "network");
                std::strcat(link.data(), ".html");
                ::ShellExecute(::GetDesktopWindow(), "open", link.data(), "", 0, SW_SHOWNORMAL);
            }
            break;
        case IDOK:
            SocketConnect = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_CONNECT_SOCK, BM_GETCHECK, 0, 0));
            HTTPConnect = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_CONNECT_HTTP, BM_GETCHECK, 0, 0));
            AllowLoopback = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_ALLOWLOOPBACK, BM_GETCHECK, 0, 0));
            LoopbackOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_LOOPBACKONLY, BM_GETCHECK, 0, 0));
            AutoPortSelect = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_PORTNO_AUTO, BM_GETCHECK, 0, 0));
            BOOL ok1, ok2;
            PortNumber = ::GetDlgItemInt(hwnd, IDC_PORTRFB, &ok1, TRUE);
            HttpPortNumber = ::GetDlgItemInt(hwnd, IDC_PORTHTTP, &ok2, TRUE);
            break;
        case IDCANCEL:
            ::EndDialog(hwnd, IDCANCEL);
            return TRUE;
        default:
            if (initdone)
            {
                SocketConnect = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_CONNECT_SOCK, BM_GETCHECK, 0, 0));
                HTTPConnect = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_CONNECT_HTTP, BM_GETCHECK, 0, 0));
                AllowLoopback = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_ALLOWLOOPBACK, BM_GETCHECK, 0, 0));
                LoopbackOnly = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_LOOPBACKONLY, BM_GETCHECK, 0, 0));
                AutoPortSelect = static_cast<LONG>(::SendDlgItemMessage(hwnd, IDC_PORTNO_AUTO, BM_GETCHECK, 0, 0));


                ::EnableWindow(::GetDlgItem(hwnd, IDC_CONNECT_HTTP), SocketConnect);
                ::EnableWindow(::GetDlgItem(hwnd, IDC_ALLOWLOOPBACK), SocketConnect);
                ::EnableWindow(::GetDlgItem(hwnd, IDC_LOOPBACKONLY), SocketConnect);
                ::EnableWindow(::GetDlgItem(hwnd, IDC_PORTRFB), (SocketConnect && !AutoPortSelect));
                ::EnableWindow(::GetDlgItem(hwnd, IDC_PORTHTTP), (SocketConnect && HTTPConnect && !AutoPortSelect));
                ::EnableWindow(::GetDlgItem(hwnd, IDC_PORTNO_AUTO), SocketConnect);
                ::EnableWindow(::GetDlgItem(hwnd, IDC_SPECPORT), SocketConnect);
                ::EnableWindow(::GetDlgItem(hwnd, IDC_UPNPON), (SocketConnect && !AutoPortSelect && UPNP_status));
                ::EnableWindow(::GetDlgItem(hwnd, IDC_UPNPOFF), (SocketConnect && !AutoPortSelect && UPNP_status));
                ::EnableWindow(::GetDlgItem(hwnd, IDC_TESTNET), (SocketConnect && !AutoPortSelect && UPNP_status));
                ::EnableWindow(::GetDlgItem(hwnd, IDC_CHECKUPNP), (SocketConnect && !AutoPortSelect));
                ::EnableWindow(::GetDlgItem(hwnd, IDC_FIXUPNP), (!UPNP_status && UPNP_status_checked));
                BOOL ok1, ok2;
                PortNumber = ::GetDlgItemInt(hwnd, IDC_PORTRFB, &ok1, TRUE);
                HttpPortNumber = ::GetDlgItemInt(hwnd, IDC_PORTHTTP, &ok2, TRUE);
            }
            break;
        }
        return 0;
    }

    return 0;
}