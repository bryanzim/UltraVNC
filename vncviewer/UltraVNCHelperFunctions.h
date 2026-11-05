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

#include "./res/resource.h"

#include <commctrl.h>

#include <array>

#if defined _M_IX86
#pragma comment(                                                                                                                   \
    linker,                                                                                                                        \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(                                                                                                                   \
    linker,                                                                                                                        \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(                                                                                                                   \
    linker,                                                                                                                        \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(                                                                                                                   \
    linker,                                                                                                                        \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#ifndef UltraVNCHelperFunctions_H__
#define UltraVNCHelperFunctions_H__
#pragma once

extern std::array<wchar_t, 128U> str50275;
extern std::array<wchar_t, 128U> str50276;
extern std::array<wchar_t, 128U> str50277;
extern std::array<wchar_t, 128U> str50278;
extern std::array<wchar_t, 128U> str50279;
extern std::array<wchar_t, 128U> str50280;
extern std::array<wchar_t, 128U> str50281;
extern std::array<wchar_t, 128U> str50282;
extern std::array<wchar_t, 128U> str50283;
extern std::array<wchar_t, 128U> str50284;
extern std::array<wchar_t, 128U> str50285;
extern std::array<wchar_t, 128U> str50286;
extern std::array<wchar_t, 128U> str50287;
extern std::array<wchar_t, 128U> str50288;
extern std::array<wchar_t, 128U> str50289;
extern std::array<wchar_t, 128U> str50290;
extern std::array<wchar_t, 128U> str50293;
extern std::array<wchar_t, 128U> str50294;
extern std::array<wchar_t, 128U> str50295;
extern std::array<wchar_t, 128U> str50296;
extern std::array<wchar_t, 128U> str50297;

void loadStrings(HINSTANCE m_hInstResDLL);
char* GetVersionFromResource(char* version);
#endif