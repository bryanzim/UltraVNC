// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 2001 Const Kaplinsky. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// SharedData.cpp : Storage for the global data in the DLL
// This file is used only when your compiler is Borland C++.

// Change default data segment and data class names
#pragma option -zRSHSEG
#pragma option -zTSHCLASS

#include <Windows.h>

DWORD vnc_thread_id{ 0U };
UINT UpdateRectMessage{ 0U };
UINT CopyRectMessage{ 0U };
UINT MouseMoveMessage{ 0U };
HHOOK hCallWndHook = nullptr;    // Handle to the CallWnd hook
HHOOK hGetMsgHook = nullptr;     // Handle to the GetMsg hook
HHOOK hDialogMsgHook = nullptr;  // Handle to the DialogMsg hook
HHOOK hLLKeyboardHook = nullptr; // Handle to LowLevel kbd hook
HHOOK hLLMouseHook = nullptr;    // Handle to LowLevel mouse hook
