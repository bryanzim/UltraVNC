// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


#if (!defined(WINVNC_VNCCAD))
#define WINVNC_VNCCAD

#include "stdhdrs.h"

#include <WinSock2.h>

#include <Windows.h>

class vncCad
{
public:
    explicit vncCad();
    static DWORD WINAPI Cadthread(LPVOID lpParam);
    static void Enable_softwareCAD_elevated();
    static bool IsSoftwareCadEnabled();
    static bool ISUACENabled();
    static void Enable_softwareCAD();
    static void delete_softwareCAD();
    static void delete_softwareCAD_elevated();

private:
};

#endif
