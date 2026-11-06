// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#pragma once
#include <Windows.h>

#include <iostream>
#include <list>
#include <stdio.h>
#include <vector>

#if !defined(__MINGW32__)
#include <swdevice.h>

#else
#include <memory>

enum SWDeviceCapabilities
{
    SWDeviceCapabilitiesRemovable = 0x1,
    SWDeviceCapabilitiesSilentInstall = 0x2,
    SWDeviceCapabilitiesDriverRequired = 0x8
};
typedef void(WINAPI* SW_DEVICE_CREATE_CALLBACK)(HANDLE, HRESULT, PVOID, PCWSTR);
using HSWDEVICE = HANDLE;
using PHSWDEVICE = HANDLE*;
using DEVPROPERTY = int;
typedef struct SW_DEVICE_CREATE_INFO_
{
    int cbSize;
    PCWSTR pszzCompatibleIds;
    PCWSTR pszInstanceId;
    PCWSTR pszzHardwareIds;
    PCWSTR pszDeviceDescription;
    int CapabilityFlags;
} SW_DEVICE_CREATE_INFO;
#endif //__MINGW32__
#include <AclAPI.h>

#include <array>
#include <map>

using namespace std;

const static LPCSTR g_szIPC = ("Global\\{4A77E11C-B0B4-40F9-AA8B-D249116A76FE}");

enum DisplayMode
{
    dmDisplay,
    dmVirtual,
    dmExtend,
    dmExtendOnly
};

typedef struct _SUPPORTEDMONITORS
{
    int counter;
    int w[200U];
    int h[200U];
} SUPPORTEDMONITORS;

typedef struct _DISPLAYINFO
{
    DEVMODE dm;
    std::array<CHAR, 256U> devicenaam;
    bool primary;
} DISPLAYINFO;

typedef struct _VIRTUALDISPLAY
{
    int clientId;
    std::array<CHAR, 256U> devicenaam;
    HSWDEVICE hDevice;
    HANDLE hEvent;
    bool singleExtendMode;
} VIRTUALDISPLAY;

typedef struct _NAMES
{
    CHAR naam[256U];
} NAMES;

typedef HRESULT(__stdcall* PSwDeviceCreate)(PCWSTR pszEnumeratorName, PCWSTR pszParentDeviceInstance,
                                            const SW_DEVICE_CREATE_INFO* pCreateInfo, ULONG cPropertyCount,
                                            const DEVPROPERTY* pProperties, SW_DEVICE_CREATE_CALLBACK pCallback, PVOID pContext,
                                            PHSWDEVICE phSwDevice);

typedef void(__stdcall* PSwDeviceClose)(HSWDEVICE hSwDevice);
typedef BOOL(WINAPI* DiInstallDriverAFn)(HWND hwndParent OPTIONAL, LPCSTR InfPath, DWORD Flags, PBOOL NeedReboot OPTIONAL);
typedef void(WINAPI* RtlGetVersion_FUNC)(OSVERSIONINFOEXW*);

class VirtualDisplay
{
private:
    LPVOID FileView;
    HANDLE hFileMap;
    SUPPORTEDMONITORS* pbuff;
    std::list<DISPLAYINFO> diplayInfoList;
    std::list<VIRTUALDISPLAY> virtualDisplayList;
    std::list<NAMES> displayList;
    bool initialized;
    bool restoreNeeded;
    HMODULE hdll;
    PSwDeviceCreate SwDeviceCreateUVNC;
    PSwDeviceClose SwDeviceCloseUVNC;

    void realMonitors(map<pair<int, int>, pair<int, int>> resolutionMap);
    void extendMonitors(map<pair<int, int>, pair<int, int>> resolutionMap, int clientId, bool singleExtendMode, char* displayName);
    void virtualMonitors(map<pair<int, int>, pair<int, int>> resolutionMap, int clientId);

    bool ContainDisplayName(char naam[256U]);
    void getSetDisplayName(char* displayName);
    void recordDisplayNames();
    void changeDisplaySize(int w, int h, char naam[256U]);
    void disconnectAllDisplays();
    void SetVirtualMonitorsSize(int height, int width);
    void AddVirtualMonitors(int clientId, bool singleExtendMode);
    bool AddVirtualDisplay(HSWDEVICE& hSwDevice, HANDLE& hEvent, WCHAR* name);
    HRESULT ChangePrimaryMonitor(char gdiDeviceName[256U]);

public:
    explicit VirtualDisplay();
    ~VirtualDisplay();
    static bool InstallDriver(bool fromCommandline);
    void attachDisplay(DisplayMode flag, map<pair<int, int>, pair<int, int>> resolutionMap, bool singleExtendMode, int clientId,
                       char* displayName);
    void disconnectDisplay(int clientId, bool lastViewer);
};
