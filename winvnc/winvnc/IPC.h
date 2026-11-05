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

#include <winsock2.h>

#include <windows.h>

#include <array>

struct mystruct
{
    short counter;
    short locked;
    std::array<RECT, 2000U> rect1;
    std::array<RECT, 2000U> rect2;
    short type[2000U];
    short test;
};

// Class for Inter Process Communication using Memory Mapped Files
class CIPC
{
public:
    explicit CIPC();
    virtual ~CIPC();

    /**unsigned char * CreateBitmap();
    unsigned char * CreateBitmap(int size);
    void CloseBitmap();*/
    bool CreateIPCMMF(void);
    bool OpenIPCMMF(void);
    void CloseIPCMMF(void);
    mystruct* listall();

    bool CreateIPCMMFBitmap(int size);
    bool OpenIPCMMFBitmap(void);
    void CloseIPCMMFBitmap(void);

    bool IsOpen(void) const { return (m_hFileMap != nullptr); }

protected:
    HANDLE m_hFileMap;
    LPVOID m_FileView;

    HANDLE m_hFileMapBitmap;
    LPVOID m_FileViewBitmap;

    mystruct list;
    mystruct* plist;
    unsigned char* pBitmap;
};
