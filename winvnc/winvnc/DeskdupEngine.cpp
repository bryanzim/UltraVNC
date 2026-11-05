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

#include "DeskdupEngine.h"

#include "winvnc.h"

#include <stdio.h>

#ifdef SC_20
#include "../loadmemory/loadDllFromMemory.h"

#include <array>

#endif // SC_20

//-----------------------------------------------------------
DeskDupEngine::DeskDupEngine()
{
    pSharedMemory = nullptr;
    pFramebuffer = nullptr;
    pChangebuf = nullptr;

    hFileMap = nullptr;
    fileView = 0;

    hFileMapBitmap = nullptr;
    fileViewBitmap = 0;

    init = true;
    hModule = nullptr;
    StartW8 = nullptr;
    StartW8V2 = nullptr;
    StopW8 = nullptr;
    LockW8 = nullptr;
    UnlockW8 = nullptr;
    ShowCursorW8 = nullptr;
    HideCursorW8 = nullptr;
    osVer = osVersion();
    if (osVer == OSWIN10)
    {
#ifdef _X64
        std::array<char, MAX_PATH> szCurrentDir;
        ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
        ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\ddengine64.dll");
#endif // _X64
#ifdef SC_20
        loadDllFromMemory = std::make_unique<LoadDllFromMemory>();
        loadDllFromMemory->loadDDengine(StartW8, StartW8V2, StopW8, LockW8, UnlockW8, ShowCursorW8, HideCursorW8);
        if ((StartW8 == nullptr) || (StopW8 == nullptr) || (LockW8 == nullptr) || (UnlockW8 == nullptr) ||
            (ShowCursorW8 == nullptr) || (HideCursorW8 == nullptr))
        {
            init = false;
        }
#else
#ifndef _X64
        std::array<char, MAX_PATH> szCurrentDir;
        ::strcpy_s(szCurrentDir.data(), szCurrentDir.size(), winvncFolder.data());
        ::strcat_s(szCurrentDir.data(), szCurrentDir.size(), "\\ddengine.dll");

#endif // _X64
        hModule = ::LoadLibrary(szCurrentDir.data());
        if (hModule)
        {
            StartW8 = (StartW8Fn)::GetProcAddress(hModule, "StartW8");
            StartW8V2 = (StartW8V2Fn)::GetProcAddress(hModule, "StartW8V2");
            StopW8 = (StopW8Fn)::GetProcAddress(hModule, "StopW8");

            LockW8 = (LockW8Fn)::GetProcAddress(hModule, "LockW8");
            UnlockW8 = (UnlockW8Fn)::GetProcAddress(hModule, "UnlockW8");

            ShowCursorW8 = (ShowCursorW8Fn)::GetProcAddress(hModule, "ShowCursorW8");
            HideCursorW8 = (HideCursorW8Fn)::GetProcAddress(hModule, "HideCursorW8");

            if ((StartW8 == nullptr) || (StopW8 == nullptr) || (LockW8 == nullptr) || (UnlockW8 == nullptr) ||
                (ShowCursorW8 == nullptr) || (HideCursorW8 == nullptr))
            {
                init = false;
            }
        }
        else
        {
            init = false;
        }
#endif // SC_20
    }

#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), "DeskDupEngine\n");
    ::OutputDebugString(szText.data());
#endif // _DEBUG
    hScreenEvent = nullptr;
    hPointerEvent = nullptr;
}
//-----------------------------------------------------------
DeskDupEngine::~DeskDupEngine()
{
#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), "~DeskDupEngine\n");
    ::OutputDebugString(szText.data());
#endif // _DEBUG
    videoDriver_Stop();
    if (osVer == OSWIN10)
    {
        if (hModule)
        {
            ::FreeLibrary(hModule);
        }
    }
}
//-----------------------------------------------------------
void DeskDupEngine::videoDriver_start(const int x, int y, int w, int h, const bool onlyVirtual, const int maxFPS)
{
#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), "DeskDupEngine Start\n");
    ::OutputDebugString(szText.data());
#endif // _DEBUG
    oldAantal = 1;

    if (!init)
    {
        return;
    }
    int refw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int refh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    bool primonly = !((refh == h) && (refw == w));

    if (StartW8V2)
    {
        if (!StartW8V2(primonly, onlyVirtual, 1000 / maxFPS))
        {
            vnclog.Print(LL_INTWARN, VNCLOG("DDengine V2 failed, not supported by Video Driver\n"));
            return;
        }
    }
    else if (StartW8)
    {
        if (!StartW8(primonly))
        {
            vnclog.Print(LL_INTWARN, VNCLOG("DDengine failed, not supported by Video Driver\n"));
            return;
        }
    }

    if (hFileMap != nullptr)
    {
        return;
    }
    hFileMap = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, g_szIPCSharedMMF);
    if (hFileMap == nullptr)
    {
        return;
    }

    fileView = ::MapViewOfFile(hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (fileView == nullptr)
    {
        return;
    }

    pChangebuf = (CHANGES_BUF*)fileView;
    pChangebuf->pointrect[0U].rect.left;

    if (hFileMapBitmap != nullptr)
    {
        return;
    }
    hFileMapBitmap = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, g_szIPCSharedMMFBitmap);
    if (hFileMapBitmap == nullptr)
    {
        return;
    }

    fileViewBitmap = ::MapViewOfFile(hFileMapBitmap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (fileViewBitmap == nullptr)
    {
        return;
    }

    if (hScreenEvent != nullptr)
    {
        return;
    }
    hScreenEvent = ::OpenEvent(EVENT_ALL_ACCESS, false, g_szIPCSharedEvent);
    if (hScreenEvent == nullptr)
    {
        return;
    }

    if (hPointerEvent != nullptr)
    {
        return;
    }
    hPointerEvent = ::OpenEvent(EVENT_ALL_ACCESS, false, g_szIPCSharedPointerEvent);
    if (hPointerEvent == nullptr)
    {
        return;
    }

    pFramebuffer = (PCHAR)fileViewBitmap;
}
//-----------------------------------------------------------
void DeskDupEngine::videoDriver_Stop()
{
#ifdef _DEBUG
    std::array<char, 256U> szText;
    ::sprintf_s(szText.data(), szText.size(), "DeskDupEngine Stop\n");
    ::OutputDebugString(szText.data());
#endif // _DEBUG
    if (!init)
    {
        return;
    }
    if (fileView)
    {
        ::UnmapViewOfFile(fileView);
    }
    fileView = nullptr;
    if (hFileMap != nullptr)
    {
        ::CloseHandle(hFileMap);
    }
    hFileMap = nullptr;
    if (fileViewBitmap)
    {
        ::UnmapViewOfFile((LPVOID)fileViewBitmap);
    }
    fileViewBitmap = nullptr;
    if (hFileMapBitmap != nullptr)
    {
        ::CloseHandle(hFileMapBitmap);
    }
    hFileMapBitmap = nullptr;
    if (hScreenEvent != nullptr)
    {
        ::CloseHandle(hScreenEvent);
    }
    if (hPointerEvent != nullptr)
    {
        ::CloseHandle(hPointerEvent);
    }
    hScreenEvent = nullptr;
    hPointerEvent = nullptr;
    StopW8();
}
//-----------------------------------------------------------
void DeskDupEngine::Lock() { LockW8(); }
//-----------------------------------------------------------
void DeskDupEngine::Unlock() { UnlockW8(); }
//-----------------------------------------------------------
bool DeskDupEngine::hardwareCursor()
{
    ShowCursorW8();
    return true;
}
//-----------------------------------------------------------
bool DeskDupEngine::noHardwareCursor()
{
    HideCursorW8();
    return true;
}
//-----------------------------------------------------------