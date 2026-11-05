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
#include "VNCOptions.h"

#include <array>

class Snapshot
{
private:
    VNCOptions m_opts;

    static BOOL CALLBACK DlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    std::array<TCHAR, MAX_PATH> m_folder;
    TCHAR m_prefix[56U];
    int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

public:
    explicit Snapshot();
    virtual ~Snapshot();
    int DoDialogSnapshot(TCHAR folder[MAX_PATH], TCHAR prefix[56U]);
    TCHAR* getFolder() { return m_folder.data(); }
    TCHAR* getPrefix() { return m_prefix; }
    void SaveJpeg(HBITMAP membit, TCHAR folder[MAX_PATH], TCHAR prefix[56U], TCHAR format[56U], const TCHAR* hostname = nullptr);
};
