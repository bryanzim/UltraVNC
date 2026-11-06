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
#ifndef tempdisplayH
#define tempdisplayH
#include <WinSock2.h>

#include <Windows.h>

#include <array>
#include <list>
#include <stdio.h>

typedef BOOL(WINAPI* ENUMDISPLAYSETTINGSEXA)(LPCSTR, DWORD, LPDEVMODEA, DWORD);
typedef LONG(WINAPI* pChangeDisplaySettingsExA)(LPCSTR, LPDEVMODEA, HWND, DWORD, LPVOID);
typedef struct _mymonitor
{
    int width;
    int height;
    int depth;
    int offsetx;
    int offsety;
    int freq;
    std::array<char, 100U> devicename;
    std::array<char, 250U> buttontext;
    int wl;
    int wr;
    int wt;
    int wb;
    HMONITOR hm;
} mymonitor;

struct MyBitmapInfo
{
    BITMAPINFOHEADER bi;
    union
    {
        std::array<RGBQUAD, 256U> colors;
        std::array<DWORD, 256U> fields;
    };
};

class tempdisplayclass
{
public:
    explicit tempdisplayclass();
    ~tempdisplayclass();
    void Init();
    void checkmonitors();
    int nr_monitors;
    std::array<mymonitor, 15U> monarray;
    int getSelectedScreen(HWND hwnd, bool allowMonitorSpanning);

private:
    HINSTANCE hUser32;
};
#endif
