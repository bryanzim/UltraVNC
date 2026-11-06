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

#include "videodriver.h"

#include <strsafe.h>

#include <array>
#include <cstdio>
#include <cstring>

LPSTR driverName = "mv video hook driver2";

#define MAP1 1030
#define UNMAP1 1031
#define CURSOREN 1060
#define CURSORDIS 1061
//----------------------------------------------------------
//----------------------------------------------------------
VideoDriver::VideoDriver()
{
    init = false;
    pSharedMemory = nullptr;
    pChangebuf = nullptr;
    pFramebuffer = nullptr;
    shared_buffer_size = 0;
    osVer = osVersion();
}
//----------------------------------------------------------
VideoDriver::~VideoDriver()
{
    if (osVer == OSWINXP)
    {
        mirror_driver_detach_XP();
        if (pSharedMemory != nullptr)
        {
            videoMemory_ReleaseSharedMemory(pSharedMemory);
        }
    }
    if (osVer == OSVISTA)
    {
        mirror_driver_Vista(0, 0, 0, 0, 0);
        if (pSharedMemory != nullptr)
        {
            videoMemory_ReleaseSharedMemory(pSharedMemory);
        }
    }
}
//----------------------------------------------------------
void VideoDriver::videoDriver_start(const int x, int y, const int w, const int h, [[maybe_unused]] const bool onlyVirtual, [[maybe_unused]] const int maxFPS)
{
    if (init)
    {
        return;
    }
    blocked = true;
    oldAantal = 1;
    pSharedMemory = nullptr;
    if (osVer == OSWINXP)
    {
        if (mirror_driver_attach_XP(x, y, w, h))
        {
            if (getDcMirror() != nullptr)
            {
                pSharedMemory = videoMemory_GetSharedMemory();
                pChangebuf = (PCHANGES_BUF)pSharedMemory;
                pFramebuffer = pSharedMemory + sizeof(CHANGES_BUF);
            }
            else
            {
                pSharedMemory = nullptr;
            }
        }
        else
        {
            pSharedMemory = nullptr;
        }
    }

    if (osVer == OSVISTA)
    {
        if (mirror_driver_Vista(1, x, y, w, h))
        {
            if (getDcMirror() != nullptr)
            {
                pSharedMemory = videoMemory_GetSharedMemory();
                pChangebuf = (PCHANGES_BUF)pSharedMemory;
                pFramebuffer = pSharedMemory + sizeof(CHANGES_BUF);
            }
            else
            {
                if (mirror_driver_attach_XP(x, y, w, h))
                {
                    if (getDcMirror() != nullptr)
                    {
                        pSharedMemory = videoMemory_GetSharedMemory();
                        pChangebuf = (PCHANGES_BUF)pSharedMemory;
                        pFramebuffer = pSharedMemory + sizeof(CHANGES_BUF);
                    }
                    else
                    {
                        pSharedMemory = nullptr;
                    }
                }
                else
                {
                    pSharedMemory = nullptr;
                }
            }
        }
        else
        {
            pSharedMemory = nullptr;
        }
    }
    blocked = false;
}
//----------------------------------------------------------
void VideoDriver::videoDriver_Stop()
{
    if (init)
    {
        return;
    }
    if (osVer == OSWINXP)
    {
        mirror_driver_detach_XP();
        if (pSharedMemory != nullptr)
        {
            videoMemory_ReleaseSharedMemory(pSharedMemory);
        }
    }
    if (osVer == OSVISTA)
    {
        mirror_driver_Vista(0, 0, 0, 0, 0);
        if (pSharedMemory != nullptr)
        {
            videoMemory_ReleaseSharedMemory(pSharedMemory);
        }
    }
    pSharedMemory = nullptr;
    pChangebuf = nullptr;
    pFramebuffer = nullptr;
}
//----------------------------------------------------------
bool VideoDriver::hardwareCursor()
{
    HDC gdc;
    int returnvalue;
    gdc = ::GetDC(nullptr);
    returnvalue = ::ExtEscape(gdc, MAP1, 0, nullptr, 0, nullptr);
    returnvalue = ::ExtEscape(gdc, CURSOREN, 0, nullptr, 0, nullptr);
    ::ReleaseDC(nullptr, gdc);
    return true;
}
//----------------------------------------------------------
bool VideoDriver::noHardwareCursor()
{
    HDC gdc;
    int returnvalue;
    gdc = ::GetDC(nullptr);
    returnvalue = ::ExtEscape(gdc, CURSORDIS, 0, nullptr, 0, nullptr);
    ::ReleaseDC(nullptr, gdc);
    return true;
}
//----------------------------------------------------------
// Private
//----------------------------------------------------------
void VideoDriver::videoMemory_ReleaseSharedMemory(const PCHAR pVideoMemory) { ::UnmapViewOfFile(pVideoMemory); }
//----------------------------------------------------------
PCHAR VideoDriver::videoMemory_GetSharedMemory(void)
{
    PCHAR pVideoMemory = nullptr;
    HANDLE hMapFile, hFile, hFile0, hFile1;
    hFile = nullptr;

    hFile0 = ::CreateFile("c:\\video0.dat", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                        0, nullptr);
    hFile1 = ::CreateFile("c:\\video1.dat", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                        0, nullptr);

    if ((hFile0 && (hFile0 != INVALID_HANDLE_VALUE)) && !(hFile1 && (hFile1 != INVALID_HANDLE_VALUE)))
    {
        hFile = hFile0;
    }
    if ((hFile1 && (hFile1 != INVALID_HANDLE_VALUE)) && !(hFile0 && (hFile0 != INVALID_HANDLE_VALUE)))
    {
        hFile = hFile1;
    }
    if ((hFile0 && (hFile0 != INVALID_HANDLE_VALUE)) && (hFile1 && (hFile1 != INVALID_HANDLE_VALUE)))
    {
        DWORD size0 = ::GetFileSize(hFile0, nullptr);
        DWORD size1 = ::GetFileSize(hFile1, nullptr);
        if (size0 == shared_buffer_size)
        {
            hFile = hFile0;
        }
        if (size1 == shared_buffer_size)
        {
            hFile = hFile1;
        }
        if ((size1 == shared_buffer_size) && (size0 == shared_buffer_size))
        {
            //find last modification time
            FILETIME createt0, lastWriteTime0, lastAccessTime0;
            ::GetFileTime(hFile0, &createt0, &lastAccessTime0, &lastWriteTime0);
            FILETIME createt1, lastWriteTime1, lastAccessTime1;
            ::GetFileTime(hFile0, &createt1, &lastAccessTime1, &lastWriteTime1);
            LONG result = ::CompareFileTime(&lastWriteTime0, &lastWriteTime1);
            if ((result == 0) || (result == 1))
            {
                hFile = hFile0;
            }
            else
            {
                hFile = hFile1;
            }
        }
    }

    if (hFile && (hFile != INVALID_HANDLE_VALUE))
    {
        hMapFile = ::CreateFileMapping(hFile, nullptr, PAGE_READWRITE, 0, 0, nullptr);
        if (hMapFile && (hMapFile != INVALID_HANDLE_VALUE))
        {
            pVideoMemory = (char*)::MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
            ::CloseHandle(hMapFile);
        }
        ::CloseHandle(hFile);
    }
    else
    {
        vnclog.Print(LL_INTERR, VNCLOG("Error video.dat \n"));
    }
    return pVideoMemory;
}
//----------------------------------------------------------
bool VideoDriver::mirror_driver_Vista(const DWORD dwAttach, int x, int y, const int w, int h)
{
    HDESK hdeskInput = nullptr;
    HDESK hdeskCurrent = nullptr;
    BOOL bED = TRUE;
    DEVMODE devmode;

    ::FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;

    // Make sure we have a display on this thread.
    BOOL change = ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);

    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

    if (change)
    {
        DISPLAY_DEVICE dispDevice;
        ::FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);
        dispDevice.cb = sizeof(DISPLAY_DEVICE);
        LPSTR deviceName = nullptr;
        devmode.dmDeviceName[0U] = '\0';
        INT devNum{ 0 };
        BOOL result;
        DWORD cxPrimary{ 0xFFFFFFFFU };
        DWORD cyPrimary{ 0xFFFFFFFFU };
        // First enumerate for Primary display device:
        while (result = ::EnumDisplayDevicesA(nullptr, devNum, &dispDevice, 0))
        {
            if (dispDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            {
                // Primary device. Find out its dmPelsWidht and dmPelsHeight.
                ::EnumDisplaySettings(dispDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devmode);
                cxPrimary = devmode.dmPelsWidth;
                cyPrimary = devmode.dmPelsHeight;
                break;
            }
            ++devNum;
        }
#if defined(MIRROR_24)
        if (devmode.dmBitsPerPel == 24)
        {
            devmode.dmBitsPerPel = 32;
        }
#endif
        if ((devmode.dmBitsPerPel != 8) && (devmode.dmBitsPerPel != 16) && (devmode.dmBitsPerPel != 32))
        {
            return false;
        }
        shared_buffer_size = devmode.dmBitsPerPel / 8 * w * h + sizeof(CHANGES_BUF);
        // error check
        if (!result)
        {
            return false;
        }
        if ((cxPrimary == 0xffffffff) || (cyPrimary == 0xffffffff))
        {
            return false;
        }

        // Enumerate again for the Mirror Driver:
        devNum = 0;
        while (result = ::EnumDisplayDevicesA(nullptr, devNum, &dispDevice, 0))
        {
            if (std::strcmp(&dispDevice.DeviceString[0U], driverName) == 0)
            {
                break;
            }
            ++devNum;
        }

        // error check
        if (!result)
        {
            return false;
        }

        std::array<CHAR, MAX_PATH> deviceNum;
        LPSTR deviceSub;

        // Simply extract 'DEVICE#' from registry key. This will depend
        // on how many mirrored devices your driver has and which ones
        // you intend to use.

        _strupr(&dispDevice.DeviceKey[0U]);
        deviceSub = strstr(&dispDevice.DeviceKey[0U], "\\DEVICE");
        if (!deviceSub)
        {
            ::StringCbCopy(&deviceNum[0U], sizeof(deviceNum), "DEVICE0");
        }
        else
        {
            ::StringCbCopy(&deviceNum[0U], sizeof(deviceNum), ++deviceSub);
        }

        // Reset the devmode for Mirror Driver use:
        const int depth = devmode.dmBitsPerPel;
        ::FillMemory(&devmode, sizeof(DEVMODE), 0);
        devmode.dmSize = sizeof(DEVMODE);
        devmode.dmDriverExtra = 0;
        devmode.dmBitsPerPel = depth;
        devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_POSITION;

        ::StringCbCopy((LPSTR)&devmode.dmDeviceName[0U], sizeof(devmode.dmDeviceName), "mv2");
        deviceName = (LPSTR)&dispDevice.DeviceName[0U];

        if (bED)
        {
            // Attach and detach information is sent via the dmPelsWidth/Height
            // of the devmode.
            //
            if (dwAttach == 0)
            {
                devmode.dmPelsWidth = 0;
                devmode.dmPelsHeight = 0;
            }
            else
            {
                devmode.dmPelsWidth = w;
                devmode.dmPelsHeight = h;
                devmode.dmPosition.x = x;
                devmode.dmPosition.y = y;
            }
            // Update the mirror device's registry data with the devmode. Dont
            // do a mode change.

            /////////////////////////
            hdeskCurrent = ::GetThreadDesktop(::GetCurrentThreadId());
            if (hdeskCurrent != nullptr)
            {
                hdeskInput = ::OpenInputDesktop(0, FALSE, MAXIMUM_ALLOWED);
                if (hdeskInput != nullptr)
                {
                    ::SetThreadDesktop(hdeskInput);
                }
            }


            INT code =
                ::ChangeDisplaySettingsEx(deviceName, &devmode, nullptr, (CDS_UPDATEREGISTRY | CDS_RESET | CDS_GLOBAL), nullptr);
            //            GetDispCode(code);
            if (code != 0)
            {
                return false;
            }
            // Now do the real mode change to take Mirror Driver changes into
            // effect.
            code = ::ChangeDisplaySettingsEx(nullptr, nullptr, nullptr, 0, nullptr);

            if (hdeskCurrent)
            {
                ::SetThreadDesktop(hdeskCurrent);
            }
            if (hdeskInput)
            {
                ::CloseDesktop(hdeskInput);
            }
            if (code != 0)
            {
                return false;
            }
            return true;
        }
        return false;
    }
    return false;
}
//----------------------------------------------------------
void VideoDriver::mirror_driver_detach_XP()
{
    HDESK hdeskInput = nullptr;
    HDESK hdeskCurrent = nullptr;
    DEVMODE devmode;

    ::FillMemory(&devmode, sizeof(DEVMODE), 0);

    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;

    BOOL change = ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);

    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    if (change)
    {
        // query all display devices in the system until we hit
        // our favourate mirrored driver, then extract the device name string
        // of the format '\\.\DISPLAY#'

        DISPLAY_DEVICE dispDevice;
        ::FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);
        dispDevice.cb = sizeof(DISPLAY_DEVICE);
        LPSTR deviceName = nullptr;
        devmode.dmDeviceName[0U] = '\0';
        INT devNum{ 0 };
        BOOL result;
        while (result = ::EnumDisplayDevicesA(nullptr, devNum, &dispDevice, 0))
        {
            if (std::strcmp(&dispDevice.DeviceString[0U], driverName) == 0)
            {
                break;
            }
            ++devNum;
        }

        if (!result)
        {
            return;
        }

        std::printf("DevNum:%d\nName:%s\nString:%s\nID:%s\nKey:%s\n\n", devNum, &dispDevice.DeviceName[0U], &dispDevice.DeviceString[0U],
               &dispDevice.DeviceID[0U], &dispDevice.DeviceKey[0U]);

        std::array<CHAR, MAX_PATH> deviceNum;
        LPSTR deviceSub;

        // Simply extract 'DEVICE#' from registry key. This will depend
        // on how many mirrored devices your driver has and which ones
        // you intend to use.

        _strupr(&dispDevice.DeviceKey[0U]);

        deviceSub = strstr(&dispDevice.DeviceKey[0U], "\\DEVICE");

        if (!deviceSub)
        {
            ::StringCbCopy(&deviceNum[0U], sizeof(deviceNum), "DEVICE0");
        }
        else
        {
            ::StringCbCopy(&deviceNum[0U], sizeof(deviceNum), ++deviceSub);
        }


        HKEY hKeyProfileMirror = (HKEY)0;
        if (::RegCreateKey(HKEY_LOCAL_MACHINE,
                         _T("SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Services\\mv2"),
                         &hKeyProfileMirror) != ERROR_SUCCESS)
        {
            return;
        }
        HKEY hKeyDevice = (HKEY)0;
        if (::RegCreateKey(hKeyProfileMirror, _T(&deviceNum[0U]), &hKeyDevice) != ERROR_SUCCESS)
        {
            return;
        }
        DWORD one{ 0U };
        if (::RegSetValueEx(hKeyDevice, _T("Attach.ToDesktop"), 0, REG_DWORD, (unsigned char*)&one, 4) != ERROR_SUCCESS)
        {
            return;
        }
        ::RegCloseKey(hKeyProfileMirror);
        ::RegCloseKey(hKeyDevice);
        deviceSub = strstr(&dispDevice.DeviceKey[0U], "SYSTEM");

        HKEY hKeyProfileMirror2 = (HKEY)0;
        if (::RegCreateKey(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current"), &hKeyProfileMirror2) !=
            ERROR_SUCCESS)
        {
            return;
        }
        HKEY hKeyDevice2 = (HKEY)0;
        if (::RegCreateKey(hKeyProfileMirror2, _T(deviceSub), &hKeyDevice2) != ERROR_SUCCESS)
        {
            return;
        }
        if (::RegSetValueEx(hKeyDevice2, _T("Attach.ToDesktop"), 0, REG_DWORD, (unsigned char*)&one, 4) != ERROR_SUCCESS)
        {
            return;
        }
        ::RegCloseKey(hKeyProfileMirror2);
        ::RegCloseKey(hKeyDevice2);

        ::FillMemory(&devmode, sizeof(DEVMODE), 0);
        devmode.dmSize = sizeof(DEVMODE);
        devmode.dmDriverExtra = 0;
        ::StringCbCopy((LPSTR)&devmode.dmDeviceName[0U], sizeof(devmode.dmDeviceName), "mv2");
        deviceName = (LPSTR)&dispDevice.DeviceName[0U];
        devmode.dmBitsPerPel = 32;
        // add 'Default.*' settings to the registry under above hKeyProfile\mirror\device
        hdeskCurrent = ::GetThreadDesktop(::GetCurrentThreadId());
        if (hdeskCurrent != nullptr)
        {
            hdeskInput = ::OpenInputDesktop(0, FALSE, MAXIMUM_ALLOWED);
            if (hdeskInput != nullptr)
            {
                ::SetThreadDesktop(hdeskInput);
            }
        }

        INT code = ::ChangeDisplaySettingsEx(deviceName, &devmode, nullptr, CDS_UPDATEREGISTRY, nullptr);
        code = ::ChangeDisplaySettingsEx(deviceName, &devmode, nullptr, 0, nullptr);
        if (hdeskCurrent)
        {
            ::SetThreadDesktop(hdeskCurrent);
        }
        if (hdeskInput)
        {
            ::CloseDesktop(hdeskInput);
        }
    }
}
//----------------------------------------------------------
bool VideoDriver::mirror_driver_attach_XP(int x, int y, const int w, int h)
{
    HDESK hdeskInput = nullptr;
    HDESK hdeskCurrent = nullptr;
    DEVMODE devmode;

    ::FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    BOOL change = ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_POSITION | DM_PELSHEIGHT;
    if ((devmode.dmBitsPerPel != 8) && (devmode.dmBitsPerPel != 16) && (devmode.dmBitsPerPel != 32))
    {
        return false;
    }
    shared_buffer_size = devmode.dmBitsPerPel / 8 * w * h + sizeof(CHANGES_BUF);
    if (change)
    {
        // query all display devices in the system until we hit
        // our favourate mirrored driver, then extract the device name string
        // of the format '\\.\DISPLAY#'

        DISPLAY_DEVICE dispDevice;
        ::FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);
        dispDevice.cb = sizeof(DISPLAY_DEVICE);
        LPSTR deviceName = nullptr;
        devmode.dmDeviceName[0U] = '\0';
        INT devNum{ 0 };
        BOOL result;
        while (result = ::EnumDisplayDevicesA(nullptr, devNum, &dispDevice, 0))
        {
            if (std::strcmp(&dispDevice.DeviceString[0U], driverName) == 0)
            {
                break;
            }
            ++devNum;
        }

        if (!result)
        {
            return false;
        }

        std::printf("DevNum:%d\nName:%s\nString:%s\nID:%s\nKey:%s\n\n", devNum, &dispDevice.DeviceName[0U], &dispDevice.DeviceString[0U],
               &dispDevice.DeviceID[0U], &dispDevice.DeviceKey[0U]);

        std::array<CHAR, MAX_PATH> deviceNum;
        LPSTR deviceSub;

        // Simply extract 'DEVICE#' from registry key. This will depend
        // on how many mirrored devices your driver has and which ones
        // you intend to use.

        _strupr(&dispDevice.DeviceKey[0U]);
        deviceSub = strstr(&dispDevice.DeviceKey[0U], "\\DEVICE");
        if (!deviceSub)
        {
            ::StringCbCopy(&deviceNum[0U], MAX_PATH, "DEVICE0");
        }
        else
        {
            ::StringCbCopy(&deviceNum[0U], MAX_PATH, ++deviceSub);
        }

        HKEY hKeyProfileMirror = (HKEY)0;
        if (::RegCreateKey(HKEY_LOCAL_MACHINE,
                         _T("SYSTEM\\CurrentControlSet\\Hardware Profiles\\Current\\System\\CurrentControlSet\\Services\\mv2"),
                         &hKeyProfileMirror) != ERROR_SUCCESS)
        {
            return false;
        }

        HKEY hKeyDevice = (HKEY)0;
        if (::RegCreateKey(hKeyProfileMirror, _T(&deviceNum[0U]), &hKeyDevice) != ERROR_SUCCESS)
        {
            return false;
        }

        DWORD one{ 1U };
        if (::RegSetValueEx(hKeyDevice, _T("Attach.ToDesktop"), 0, REG_DWORD, (unsigned char*)&one, 4) != ERROR_SUCCESS)
        {
            return false;
        }
        const int depth = devmode.dmBitsPerPel;

        ::FillMemory(&devmode, sizeof(DEVMODE), 0);
        devmode.dmSize = sizeof(DEVMODE);
        devmode.dmDriverExtra = 0;
        ::StringCbCopy((LPSTR)&devmode.dmDeviceName[0U], 32, "mv2");
        devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_POSITION | DM_PELSHEIGHT;
        deviceName = (LPSTR)&dispDevice.DeviceName[0U];
        devmode.dmPelsWidth = w;
        devmode.dmPelsHeight = h;
        devmode.dmPosition.x = x;
        devmode.dmPosition.y = y;
        devmode.dmBitsPerPel = depth;

        hdeskCurrent = ::GetThreadDesktop(::GetCurrentThreadId());
        if (hdeskCurrent != nullptr)
        {
            hdeskInput = ::OpenInputDesktop(0, FALSE, MAXIMUM_ALLOWED);
            if (hdeskInput != nullptr)
            {
                ::SetThreadDesktop(hdeskInput);
            }
        }
        if (::ChangeDisplaySettingsEx(deviceName, &devmode, nullptr, CDS_UPDATEREGISTRY, nullptr) != 0)
        {
            return false;
        }

        int code = ::ChangeDisplaySettingsEx(deviceName, &devmode, nullptr, 0, nullptr);
        if (hdeskCurrent)
        {
            ::SetThreadDesktop(hdeskCurrent);
        }
        if (hdeskInput)
        {
            ::CloseDesktop(hdeskInput);
        }
        ::RegCloseKey(hKeyProfileMirror);
        ::RegCloseKey(hKeyDevice);
        if (code != 0)
        {
            return false;
        }
        return true;
    }
    return false;
}
//----------------------------------------------------------
HDC VideoDriver::getDcMirror()
{
    HDC m_hrootdc = nullptr;
    LPSTR driverNameLocal = "mv video hook driver2";
    BOOL DriverFound;
    DEVMODE devmode;
    ::FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

    LPSTR deviceName = nullptr;
    DISPLAY_DEVICE dd;
    ::ZeroMemory(&dd, sizeof(dd));
    dd.cb = sizeof(dd);
    devmode.dmDeviceName[0U] = '\0';
    INT devNum{ 0 };
    BOOL result;
    DriverFound = false;
    while (result = ::EnumDisplayDevicesA(nullptr, devNum, &dd, 0))
    {
        if (std::strcmp((const char*)&dd.DeviceString[0U], driverNameLocal) == 0)
        {
            DriverFound = true;
            break;
        }
        ++devNum;
    }
    if (DriverFound)
    {
        deviceName = (LPSTR)&dd.DeviceName[0U];
        m_hrootdc = ::CreateDC("DISPLAY", deviceName, nullptr, nullptr);
        if (m_hrootdc)
        {
            ::DeleteDC(m_hrootdc);
        }
    }
    return m_hrootdc;
}
//----------------------------------------------------------
