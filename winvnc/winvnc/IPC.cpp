// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#include "IPC.h"

#include <WinSock2.h>

#include <Windows.h>

#include <crtdbg.h>
#include <tchar.h>

const static LPCTSTR g_szIPCSharedMMF = _T("{34F673E0-878F-11D5-B98A-00B0D07B8C7C}");
const static LPCTSTR g_szIPCSharedMMFBitmap = _T("{34F673E0-878F-11D5-B98A-57B0D07B8C7C}");
int oldcounter{ 0 };
//***********************************************
CIPC::CIPC()
{
    m_hFileMap = nullptr;
    m_hFileMapBitmap = nullptr;
    pBitmap = nullptr;
    plist = nullptr;
    m_FileView = nullptr;
    m_FileViewBitmap = nullptr;
    CreateIPCMMF();
    OpenIPCMMF();
}

//***********************************************
CIPC::~CIPC()
{
    CloseIPCMMF();
    CloseIPCMMFBitmap();
}
//***********************************************
bool CIPC::CreateIPCMMF(void)
{
    bool bCreated{ false };
    int listsize = sizeof(mystruct);
    try
    {
        if (m_hFileMap != nullptr)
        {
            return false;
        } // Already created

        // Create an in-memory 4KB memory mapped file to share data
        m_hFileMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(listsize), g_szIPCSharedMMF);
        if (m_hFileMap != nullptr)
        {
            bCreated = true;
        }
    }
    catch (...)
    {
    }

    return bCreated;
}
//***********************************************
bool CIPC::CreateIPCMMFBitmap(const int size)
{
    bool bCreatedBitmap{ false };

    try
    {
        if (m_hFileMapBitmap != nullptr)
        {
            return false;
        } // Already created

        // Create an in-memory 4KB memory mapped file to share data
        m_hFileMapBitmap = ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), g_szIPCSharedMMFBitmap);
        if (m_hFileMapBitmap != nullptr)
        {
            bCreatedBitmap = true;
        }
    }
    catch (...)
    {
    }

    return bCreatedBitmap;
}

//***********************************************
bool CIPC::OpenIPCMMF(void)
{
    bool bOpened{ false };

    try
    {
        if (m_hFileMap == nullptr)
        {

            m_hFileMap = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, g_szIPCSharedMMF);
        }

        if (m_hFileMap != nullptr)
        {
            bOpened = true;
            m_FileView = ::MapViewOfFile(m_hFileMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
            if (m_FileView == nullptr)
            {
                bOpened = false;
            }
            plist = reinterpret_cast<mystruct*>(m_FileView);
        }
    }
    catch (...)
    {
    }

    return bOpened;
}
//***********************************************
bool CIPC::OpenIPCMMFBitmap(void)
{
    bool bOpenedBitmap{ false };

    try
    {
        if (m_hFileMapBitmap == nullptr)
        {

            m_hFileMapBitmap = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, g_szIPCSharedMMFBitmap);
        }

        if (m_hFileMapBitmap != nullptr)
        {
            bOpenedBitmap = true;
            m_FileViewBitmap = ::MapViewOfFile(m_hFileMapBitmap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
            if (m_FileViewBitmap == nullptr)
            {
                bOpenedBitmap = false;
            }
            pBitmap = reinterpret_cast<byte*>(m_FileViewBitmap);
        }
    }
    catch (...)
    {
    }

    return bOpenedBitmap;
}
//***********************************************
void CIPC::CloseIPCMMF(void)
{
    try
    {
        if (m_FileView)
        {
            ::UnmapViewOfFile(m_FileView);
        }
        if (m_hFileMap != nullptr)
        {
            ::CloseHandle(m_hFileMap), m_hFileMap = nullptr;
        }
    }
    catch (...)
    {
    }
}
//***********************************************
void CIPC::CloseIPCMMFBitmap(void)
{
    try
    {
        if (m_FileViewBitmap)
        {
            ::UnmapViewOfFile(reinterpret_cast<LPVOID>(m_FileViewBitmap));
        }
        m_FileViewBitmap = nullptr;
        if (m_hFileMapBitmap != nullptr)
        {
            ::CloseHandle(m_hFileMapBitmap);
        }
        m_hFileMapBitmap = nullptr;
    }
    catch (...)
    {
    }
}
mystruct* CIPC::listall() { return plist; }
//***********************************************
