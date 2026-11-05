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
#include "stdhdrs.h"

class DlgChangePassword
{
private:
    HWND DlgChangePasswordHwnd = nullptr;
    char title[1024U]{};
    int passwordSize;
    char passwd[1024U];

public:
    explicit DlgChangePassword();
    int ShowDlg(HWND parentWindow, const char* title, int passwordSize);
    ~DlgChangePassword();
    bool InitDialog(HWND hwnd);
    void onOK(HWND hwnd);
    char* getPassword();
};
