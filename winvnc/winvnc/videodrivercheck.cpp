// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include <WinSock2.h>

#include <Windows.h>

#include "Localization.h"
#include "ScreenCapture.h"
#include "vncOSVersion.h"

#include <array>
#include <cstring>
#include <stdlib.h>

DWORD MessageBoxSecure(HWND hWnd, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);
typedef BOOL(WINAPI* pEnumDisplayDevices)(PVOID, DWORD, PVOID, DWORD);

/*bool CheckDriver2(void)
{
    SC_HANDLE       schMaster;
    SC_HANDLE       schSlave;

    schMaster = OpenSCManager (NULL, NULL, SC_MANAGER_ALL_ACCESS);

    schSlave = OpenService (schMaster, "vnccom", SERVICE_ALL_ACCESS);

    if (schSlave == NULL)
    {
     CloseServiceHandle(schMaster);
     return false;
    }
    else
    {
        CloseServiceHandle (schSlave);
        CloseServiceHandle(schMaster);
        return true;
    }
}*/


///////////////////////////////////////////////////////////////////
BOOL GetDllProductVersion(char* const dllName, char* vBuffer, const int size)
{
    char* versionInfo;
    void* lpBuffer;
    unsigned int cBytes;
    DWORD rBuffer;

    if (!dllName || !vBuffer)
    {
        return (FALSE);
    }

    DWORD sVersion = ::GetFileVersionInfoSize(dllName, &rBuffer);
    if (sVersion == 0)
    {
        ::strcpy_s(vBuffer, 512, "Fail: Using 32-bit UltraVNC Server winvnc.exe with a 64-bit driver? \n");
        return (FALSE);
    }


    versionInfo = new char[sVersion];

    ::GetFileVersionInfo(dllName, 0, sVersion, versionInfo);

    BOOL resultValue = ::VerQueryValue(versionInfo,

                                     TEXT("\\StringFileInfo\\040904b0\\ProductVersion"), &lpBuffer, &cBytes);

    if (!resultValue)
    {
        resultValue = ::VerQueryValue(versionInfo, TEXT("\\StringFileInfo\\000004b0\\ProductVersion"), &lpBuffer, &cBytes);
    }

    if (resultValue)
    {
        ::strncpy_s(vBuffer, 512, reinterpret_cast<char*>(lpBuffer), static_cast<rsize_t>(size));
        delete[] versionInfo;
        return (TRUE);
    }
    else
    {
        *vBuffer = '\0';
        delete[] versionInfo;
        return (FALSE);
    }
}

///////////////////////////////////////////////////////////////////

bool CheckVideoDriver(const bool Box)
{
    if (IsWindows8OrGreater())
    {
        return true;
    }
    typedef BOOL(WINAPI * pEnumDisplayDevices)(PVOID, DWORD, PVOID, DWORD);
    pEnumDisplayDevices pd = nullptr;
    LPSTR driverName = "mv video hook driver2";
    BOOL DriverFound;
    DEVMODE devmode;
    ::FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    HMODULE hUser32 = ::LoadLibraryEx("USER32", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (hUser32)
    {
        pd = reinterpret_cast<pEnumDisplayDevices>(::GetProcAddress(hUser32, "EnumDisplayDevicesA"));
    }
    if (pd)
    {
        LPSTR deviceName = nullptr;
        DISPLAY_DEVICE dd;
        ::ZeroMemory(&dd, sizeof(dd));
        dd.cb = sizeof(dd);
        devmode.dmDeviceName[0U] = '\0';
        INT devNum{ 0 };
        BOOL result;
        DriverFound = false;
        while (result = (*pd)(nullptr, static_cast<DWORD>(devNum), &dd, 0))
        {
            if (std::strcmp(reinterpret_cast<const char*>(&dd.DeviceString[0U]), driverName) == 0)
            {
                DriverFound = true;
                break;
            }
            ++devNum;
        }
        if (DriverFound)
        {
            if (hUser32)
            {
                ::FreeLibrary(hUser32);
            }
            if (Box)
            {
                std::array<char, 512U> buf;
                std::array<char, 512U> buf2;
                ::strcpy_s(buf.data(), buf.size(), "");
                ::strcpy_s(buf2.data(), buf2.size(), "");
                ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_FOUND.data());
                GetDllProductVersion("mv2.dll", buf.data(), 512);
                if (std::strcmp(buf.data(), "1.00.22") == 0)
                {
                    ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_VERSION_OK.data());
                }
                else
                {
                    ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_VERSION_NOT_OK.data());
                    ::strcat_s(buf2.data(), buf2.size(), buf.data());
                    ::strcat_s(buf2.data(), buf2.size(), " \n");
                }

                if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
                {
                    ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_ACTIVE.data());
                    HDC testdc = nullptr;
                    deviceName = reinterpret_cast<LPSTR>(&dd.DeviceName[0U]);
                    testdc = ::CreateDC("DISPLAY", deviceName, nullptr, nullptr);
                    if (testdc)
                    {
                        ::DeleteDC(testdc);
                        ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_ACCESS_OK.data());
                    }
                    else
                    {
                        ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_ACCESS_DENIED.data());
                    }
                }
                else
                {
                    ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_NOT_ACTIVATED.data());
                    ::strcat_s(buf2.data(), buf2.size(), sz_ID_DRIVER_ONLY_SERVICE_ADMIN.data());
                }
                MessageBoxSecure(nullptr, buf2.data(), buf.data(), 0);
            }
            return true;
            //deviceName = (LPSTR)&dd.DeviceName[0];
            //m_hrootdc = CreateDC("DISPLAY",deviceName,NULL,NULL);
            //if (m_hrootdc) DeleteDC(m_hrootdc);
        }
        else if (Box)
        {
            MessageBoxSecure(nullptr, "Driver not found: Perhaps you need to reboot after install",
                             "Driver Info: Required version 1.00.22", 0);
        }
    }
    if (hUser32)
    {
        ::FreeLibrary(hUser32);
    }
    return false;
}
