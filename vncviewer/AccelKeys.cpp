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

#include "AccelKeys.h"

#include "vncviewer.h"

#include <array>
//
// Build the Accelerators Table
//

AccelKeys::AccelKeys()
{
    std::array<ACCEL, 16U> AccelTable;
    int i{ 0 };
    m_hWnd = nullptr;

    /*
    AccelTable[i].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[i].key = VK_F3;
    AccelTable[i++].cmd = ID_CONN_CTLESC;
    */

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F3;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_VIEWONLYTOGGLE;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F4;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_CONN_CTLALTDEL;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F5;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_CONN_SAVE_AS;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F6;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = IDC_OPTIONBUTTON;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F7;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_FILETRANSFER;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F8;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_TEXTCHAT;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F9;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_DBUTTON; // Toolbar Toggle

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F10;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_AUTOSCALING;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F11;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_NORMALSCREEN2;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F11;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_HALFSCREEN;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT;
    AccelTable[static_cast<unsigned long long>(i)].key = VK_F12;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_FULLSCREEN;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT | FSHIFT;
    AccelTable[static_cast<unsigned long long>(i)].key = 0x49;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_CONN_ABOUT;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT | FSHIFT;
    AccelTable[static_cast<unsigned long long>(i)].key = 0x52;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_REQUEST_REFRESH;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT | FSHIFT;
    AccelTable[static_cast<unsigned long long>(i)].key = 0x4E;
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_NEWCONN;

    AccelTable[static_cast<unsigned long long>(i)].fVirt = FVIRTKEY | FCONTROL | FALT | FNOINVERT | FSHIFT;
    AccelTable[static_cast<unsigned long long>(i)].key = 0x56; // V: type the clipboard as keystrokes
    AccelTable[static_cast<unsigned long long>(i++)].cmd = ID_CONN_TYPECLIPBOARD;

    m_hAccelTable = ::CreateAcceleratorTable(reinterpret_cast<LPACCEL>(AccelTable.data()), i);
}

//
//
//
bool AccelKeys::TranslateAccelKeys(MSG* const pMsg)
{
    if (m_hWnd == nullptr)
    {
        return false;
    }
    else
    {
        return (::TranslateAccelerator(m_hWnd, m_hAccelTable, pMsg) != 0);
    }
}


//
//
//
AccelKeys::~AccelKeys() { ::DestroyAcceleratorTable(m_hAccelTable); }
