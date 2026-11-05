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

#include "vncOSVersion.h"

#include <windows.h>

#include <array>
#include <cstring>
#include <stdlib.h>

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

    //   DWORD sName = GetModuleFileName(NULL, fileName, sizeof(fileName));
    // FYI only
    DWORD sVersion = ::GetFileVersionInfoSize(dllName, &rBuffer);
    if (sVersion == 0)
    {
        return (FALSE);
    }
    versionInfo = new char[sVersion];

    const BOOL resultVersion = ::GetFileVersionInfo(dllName, 0, sVersion, versionInfo);

    BOOL resultValue = ::VerQueryValue(versionInfo,

                                     TEXT("\\StringFileInfo\\040904b0\\ProductVersion"), &lpBuffer, &cBytes);

    if (!resultValue)
    {
        resultValue = ::VerQueryValue(versionInfo, TEXT("\\StringFileInfo\\000004b0\\ProductVersion"), &lpBuffer, &cBytes);
    }

    if (resultValue)
    {
        std::strncpy(vBuffer, (char*)lpBuffer, size);
        delete versionInfo;
        return (TRUE);
    }
    else
    {
        *vBuffer = '\0';
        delete versionInfo;
        return (FALSE);
    }
}

///////////////////////////////////////////////////////////////////

bool CheckVideoDriver(const bool Box)
{
    typedef BOOL(WINAPI * pEnumDisplayDevices)(PVOID, DWORD, PVOID, DWORD);
    HDC m_hrootdc = nullptr;
    pEnumDisplayDevices pd;
    LPSTR driverName = "mv video hook driver2";
    BOOL DriverFound;
    DEVMODE devmode;
    ::FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    const BOOL change = ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    HMODULE hUser32 = ::LoadLibrary("USER32");
    pd = (pEnumDisplayDevices)::GetProcAddress(hUser32, "EnumDisplayDevicesA");
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
        while (result = (*pd)(nullptr, devNum, &dd, 0))
        {
            if (std::strcmp((const char*)&dd.DeviceString[0U], driverName) == 0)
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
                std::strcpy(buf.data(), "");
                std::strcpy(buf2.data(), "");
                std::strcpy(buf2.data(), "Driver found. \n");
                GetDllProductVersion("mv2.dll", buf.data(), 512);
                if (std::strcmp(buf.data(), "1.00.22") == 0)
                {
                    std::strcat(buf2.data(), "Driver version OK \n");
                }
                else
                {
                    std::strcat(buf2.data(), "Driver version is not 1.00.22 \n");
                    std::strcat(buf2.data(), buf.data());
                    std::strcat(buf2.data(), " \n");
                }

                if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
                {
                    std::strcat(buf2.data(), " driver Active");
                    HDC testdc = nullptr;
                    deviceName = (LPSTR)&dd.DeviceName[0U];
                    testdc = ::CreateDC("DISPLAY", deviceName, nullptr, nullptr);
                    if (testdc)
                    {
                        ::DeleteDC(testdc);
                        std::strcat(buf2.data(), " access ok");
                    }
                    else
                    {
                        std::strcat(buf2.data(), " access denied, permission problem");
                    }
                }
                else
                {
                    std::strcat(buf2.data(), "Driver Not Activated, is the viewer current connected?\n");
                    std::strcat(buf2.data(),
                           "A Mirrior Driver can only be started if UltraVNC Server is a service or running as admin\n");
                }
                ::MessageBox(nullptr, buf2.data(), buf.data(), 0);
            }
            return true;
        }
        else if (Box)
        {
            ::MessageBox(nullptr, "Driver not found: Perhaps you need to reboot after install",
                       "Driver info: required version 1.00.22", 0);
        }
    }
    if (hUser32)
    {
        ::FreeLibrary(hUser32);
    }
    return false;
}
