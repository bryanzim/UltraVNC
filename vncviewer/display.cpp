// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "display.h"

void tempdisplayclass::checkmonitors()
{

    DISPLAY_DEVICE dd;
    ::ZeroMemory(&dd, sizeof(dd));
    dd.cb = sizeof(dd);
    DWORD dev{ 0U }; // device index
    int id{ 2 };     // monitor number, as used by Display Properties > Settings
    nr_monitors = 1;

    while (EnumDisplayDevices(nullptr, dev, &dd, 0))
    {
        if (!(dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
            if ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            {
                // ignore virtual mirror displays

                // get information about the monitor attached to this display adapter. dualhead cards
                // and laptop video cards can have multiple monitors attached
                DISPLAY_DEVICE ddMon;
                ::ZeroMemory(&ddMon, sizeof(ddMon));
                ddMon.cb = sizeof(ddMon);
                DWORD devMon{ 0U };

                // please note that this enumeration may not return the correct monitor if multiple monitors
                // are attached. this is because not all display drivers return the ACTIVE flag for the monitor
                // that is actually active
                while (EnumDisplayDevices(dd.DeviceName, devMon, &ddMon, 0))
                {
                    if (ddMon.StateFlags & DISPLAY_DEVICE_ACTIVE)
                    {
                        break;
                    }

                    ++devMon;
                }

                if (!*ddMon.DeviceString)
                {
                    EnumDisplayDevices(dd.DeviceName, 0, &ddMon, 0);
                    if (!*ddMon.DeviceString)
                    {
                        lstrcpyW(ddMon.DeviceString, L"Default Monitor");
                    }
                }

                // get information about the display's position and the current display mode
                DEVMODE dm;
                ::ZeroMemory(&dm, sizeof(dm));
                dm.dmSize = sizeof(dm);
                dm.dmDriverExtra = 0;
                if (::EnumDisplaySettingsExW(reinterpret_cast<LPCWSTR>(dd.DeviceName), ENUM_CURRENT_SETTINGS, &dm, 0) == FALSE)
                {
                    ::EnumDisplaySettingsExW(reinterpret_cast<LPCWSTR>(dd.DeviceName), ENUM_REGISTRY_SETTINGS, &dm, 0);
                }


                // get the monitor handle and workspace
                HMONITOR hm = nullptr;
                MONITORINFO mi;
                ::ZeroMemory(&mi, sizeof(mi));
                mi.cbSize = sizeof(mi);
                if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP)
                {
                    // display is enabled. only enabled displays have a monitor handle
                    POINT pt{ dm.dmPosition.x, dm.dmPosition.y };
                    hm = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
                    if (hm)
                    {
                        GetMonitorInfo(hm, &mi);
                    }
                }
                if (hm)
                {
                    if (dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP && dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
                    {
                        monarray[1U].wl = mi.rcWork.left;
                        monarray[1U].wt = mi.rcWork.top;
                        monarray[1U].wr = mi.rcWork.right;
                        monarray[1U].wb = mi.rcWork.bottom;
                        monarray[1U].hm = hm;
                        ::sprintf_s(monarray[1U].buttontext.data(), monarray[1U].buttontext.size(), "%d. %d x %d @ %d,%d - %d-bit - %d Hz", 1, dm.dmPelsWidth,
                                  dm.dmPelsHeight, dm.dmPosition.x, dm.dmPosition.y, dm.dmBitsPerPel, dm.dmDisplayFrequency);
                        monarray[1U].width = static_cast<int>(dm.dmPelsWidth);
                        monarray[1U].height = static_cast<int>(dm.dmPelsHeight);
                        monarray[1U].depth = static_cast<int>(dm.dmBitsPerPel);
                        monarray[1U].offsetx = dm.dmPosition.x;
                        monarray[1U].offsety = dm.dmPosition.y;
                        monarray[1U].freq = static_cast<int>(dm.dmDisplayFrequency);
                        ::strcpy_s(monarray[1U].devicename.data(), monarray[1U].devicename.size(), reinterpret_cast<char*>(dd.DeviceName));
                    }
                    else
                    {
                        monarray[static_cast<unsigned long long>(id)].wl = mi.rcWork.left;
                        monarray[static_cast<unsigned long long>(id)].wt = mi.rcWork.top;
                        monarray[static_cast<unsigned long long>(id)].wr = mi.rcWork.right;
                        monarray[static_cast<unsigned long long>(id)].wb = mi.rcWork.bottom;
                        monarray[static_cast<unsigned long long>(id)].hm = hm;
                        ::sprintf_s(monarray[static_cast<unsigned long long>(id)].buttontext.data(), monarray[static_cast<unsigned long long>(id)].buttontext.size(), "%d. %d x %d @ %d,%d - %d-bit - %d Hz", id, dm.dmPelsWidth,
                                  dm.dmPelsHeight, dm.dmPosition.x, dm.dmPosition.y, dm.dmBitsPerPel, dm.dmDisplayFrequency);
                        monarray[static_cast<unsigned long long>(id)].width = static_cast<int>(dm.dmPelsWidth);
                        monarray[static_cast<unsigned long long>(id)].height = static_cast<int>(dm.dmPelsHeight);
                        monarray[static_cast<unsigned long long>(id)].depth = static_cast<int>(dm.dmBitsPerPel);
                        monarray[static_cast<unsigned long long>(id)].offsetx = dm.dmPosition.x;
                        monarray[static_cast<unsigned long long>(id)].offsety = dm.dmPosition.y;
                        monarray[static_cast<unsigned long long>(id)].freq = static_cast<int>(dm.dmDisplayFrequency);
                        ::strcpy_s(monarray[static_cast<unsigned long long>(id)].devicename.data(), monarray[static_cast<unsigned long long>(id)].devicename.size(), reinterpret_cast<char*>(dd.DeviceName));
                        nr_monitors = id;
                        ++id;
                    }
                }
            }
        ++dev;
    }
    //if (nr_monitors>1)
    {
        monarray[0U].width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        monarray[0U].height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        monarray[0U].depth = monarray[1U].depth;
        monarray[0U].offsetx = GetSystemMetrics(SM_XVIRTUALSCREEN);
        monarray[0U].offsety = GetSystemMetrics(SM_YVIRTUALSCREEN);
        ::strcpy_s(monarray[0U].devicename.data(), monarray[0U].devicename.size(), "All displays");
        ::sprintf_s(monarray[0U].buttontext.data(), monarray[0U].buttontext.size(), "%d. %d x %d @ %d,%d - %d-bit ", 0, monarray[0U].width,
                  monarray[0U].height,
                  monarray[0U].offsetx, monarray[0U].offsety, monarray[0U].depth);

        RECT workrect{ 0, 0, 0, 0 };
        ::SystemParametersInfo(SPI_GETWORKAREA, 0, &workrect, 0);

        /* Update work rectangle to use the virtual screen size */
        if (monarray[0U].offsetx < 0)
        {
            workrect.left = monarray[0U].offsetx;
        }
        if (monarray[0U].offsety < 0)
        {
            workrect.top = monarray[0U].offsetx;
        }
        workrect.right = workrect.left + monarray[0U].width - (monarray[1U].width - (monarray[1U].wr - monarray[1U].wl));
        workrect.bottom = workrect.top + monarray[0U].height - (monarray[1U].height - (monarray[1U].wb - monarray[1U].wt));

        monarray[0U].wl = workrect.left;
        monarray[0U].wt = workrect.top;
        monarray[0U].wr = workrect.right;
        monarray[0U].wb = workrect.bottom;
    }
}

tempdisplayclass::tempdisplayclass()
{
    nr_monitors = 0;
    Init();
}

tempdisplayclass::~tempdisplayclass() { ::FreeLibrary(hUser32); }

void tempdisplayclass::Init()
{
    hUser32 = ::LoadLibraryExW(L"user32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    checkmonitors();
}

int tempdisplayclass::getSelectedScreen(const HWND hwnd, const bool allowMonitorSpanning)
{
    if (allowMonitorSpanning)
    {
        return 0;
    }
    HMONITOR hm = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    for (int i{ 0 }; i <= nr_monitors; ++i)
    {
        if (monarray[static_cast<unsigned long long>(i)].hm == hm)
        {
            return i;
        }
    }
    return 0;
}
