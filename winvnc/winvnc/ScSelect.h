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
#ifdef SC_20
#include "stdhdrs.h"

#include <CommCtrl.h>

#include <array>

namespace ScSelect
{
    extern std::array<wchar_t, 150U> Balloon1Title;
    extern std::array<wchar_t, 150U> Balloon2Title;
    extern std::array<wchar_t, 150U> Balloon1A;
    extern std::array<wchar_t, 150U> Balloon1B;
    extern std::array<wchar_t, 150U> Balloon1C;
    extern std::array<wchar_t, 150U> Balloon2A;
    extern std::array<wchar_t, 150U> Balloon2B;
    extern std::array<wchar_t, 150U> Balloon2C;
    extern LONG old_pref;
    extern bool g_dis_uac;
    extern bool g_wallpaper_enabled;
    extern std::array<char, 15U> idStr;
    int toUnicode(wchar_t* buffer, const char* str);
    void Disbale_UAC_for_admin_run_elevated();
    void Disbale_UAC_for_admin();
    void Restore_UAC_for_admin_elevated();
    void Restore_UAC_for_admin(int i);
    HBITMAP DoGetBkGndBitmap(IN CONST UINT uBmpResId);
    HBITMAP DoGetBkGndBitmap3(IN CONST UINT uBmpResId);
    BOOL DoSDKEraseBkGnd(IN CONST HDC hDC, IN CONST COLORREF crBkGndFill);
    BOOL CALLBACK DialogProc(HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam);
    char* InitSC(HINSTANCE hInstance, PSTR szCmdLine);
    void ExpandBox(HWND hDlg, bool fExpand);
};
#endif // SC_20
