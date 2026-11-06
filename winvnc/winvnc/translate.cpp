// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


/*
 * translate.c - translate between different pixel formats
 */

#include "stdhdrs.h"

#include "translate.h"

#include "rfb.h"
#include "vncOSVersion.h"

#include <cstring>
#include <stdio.h>

#define CONCAT2(a, b) a##b
#define CONCAT2E(a, b) CONCAT2(a, b)
#define CONCAT4(a, b, c, d) a##b##c##d
#define CONCAT4E(a, b, c, d) CONCAT4(a, b, c, d)

#define OUTVNC 8
#include "tableinitcmtemplate.cpp"
#include "tableinittctemplate.cpp"

#define INVNC 8
#include "tabletranstemplate.cpp"

#undef INVNC
#define INVNC 16
#include "tabletranstemplate.cpp"

#undef INVNC
#define INVNC 32
#include "tabletranstemplate.cpp"

#undef INVNC
#undef OUTVNC

#define OUTVNC 16
#include "tableinitcmtemplate.cpp"
#include "tableinittctemplate.cpp"

#define INVNC 8
#include "tabletranstemplate.cpp"

#undef INVNC
#define INVNC 16
#include "tabletranstemplate.cpp"

#undef INVNC
#define INVNC 32
#include "tabletranstemplate.cpp"

#undef INVNC
#undef OUTVNC

#define OUTVNC 32
#include "tableinitcmtemplate.cpp"
#include "tableinittctemplate.cpp"

#define INVNC 8
#include "tabletranstemplate.cpp"

#undef INVNC
#define INVNC 16
#include "tabletranstemplate.cpp"

#undef INVNC
#define INVNC 32
#include "tabletranstemplate.cpp"

#include <array>

#undef INVNC
#undef OUTVNC

rfbInitTableFnType rfbInitTrueColourSingleTableFns[]{ rfbInitTrueColourSingleTable8, rfbInitTrueColourSingleTable16,
                                                             rfbInitTrueColourSingleTable32 };

rfbInitTableFnType rfbInitColourMapSingleTableFns[]{ rfbInitColourMapSingleTable8, rfbInitColourMapSingleTable16,
                                                      rfbInitColourMapSingleTable32 };

rfbInitTableFnType rfbInitTrueColourRGBTablesFns[]{ rfbInitTrueColourRGBTables8, rfbInitTrueColourRGBTables16,
                                                     rfbInitTrueColourRGBTables32 };

rfbTranslateFnType rfbTranslateWithSingleTableFns[3U][3U] = {
    { rfbTranslateWithSingleTable8to8, rfbTranslateWithSingleTable8to16, rfbTranslateWithSingleTable8to32 },
    { rfbTranslateWithSingleTable16to8, rfbTranslateWithSingleTable16to16, rfbTranslateWithSingleTable16to32 },
    { rfbTranslateWithSingleTable32to8, rfbTranslateWithSingleTable32to16, rfbTranslateWithSingleTable32to32 }
};

rfbTranslateFnType rfbTranslateWithRGBTablesFns[3U][3U] = {
    { rfbTranslateWithRGBTables8to8, rfbTranslateWithRGBTables8to16, rfbTranslateWithRGBTables8to32 },
    { rfbTranslateWithRGBTables16to8, rfbTranslateWithRGBTables16to16, rfbTranslateWithRGBTables16to32 },
    { rfbTranslateWithRGBTables32to8, rfbTranslateWithRGBTables32to16, rfbTranslateWithRGBTables32to32 }
};



// rfbTranslateNone is used when no translation is required.

void rfbTranslateNone([[maybe_unused]] char* const table, [[maybe_unused]] rfbPixelFormat* const in, rfbPixelFormat* const out, char* iptr, char* optr,
                      const int bytesBetweenInputLines, const int width, int height)
{
    int bytesPerOutputLine = width * (out->bitsPerPixel >> 3);

    while (height > 0)
    {
        std::memcpy(optr, iptr, bytesPerOutputLine);
        iptr += bytesBetweenInputLines;
        optr += bytesPerOutputLine;
        --height;
    }
}



HDC GetDcMirror()
{
    typedef BOOL(WINAPI * pEnumDisplayDevices)(PVOID, DWORD, PVOID, DWORD);
    HDC m_hrootdc = nullptr;
    pEnumDisplayDevices pd = nullptr;
    LPSTR driverName = "mv video hook driver2";
    BOOL DriverFound;
    DEVMODE devmode;
    ::FillMemory(&devmode, sizeof(DEVMODE), 0);
    devmode.dmSize = sizeof(DEVMODE);
    devmode.dmDriverExtra = 0;
    ::EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    HMODULE hUser32 = ::LoadLibrary("USER32");
    if (hUser32)
    {
        pd = (pEnumDisplayDevices)::GetProcAddress(hUser32, "EnumDisplayDevicesA");
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
            deviceName = (LPSTR)&dd.DeviceName[0U];
            m_hrootdc = ::CreateDC("DISPLAY", deviceName, nullptr, nullptr);
        }
    }
    if (hUser32)
    {
        ::FreeLibrary(hUser32);
    }

    return m_hrootdc;
}