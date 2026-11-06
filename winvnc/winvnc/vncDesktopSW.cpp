// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// System headers
#include "stdhdrs.h"

#include "rfbRect.h"
#include "rfbRegion.h"
#include "vncdesktop.h"
#include "vnchooks/VNCHooks.h"
#include "vncserver.h"
#include "winvnc.h"

// Custom headers
#include <omnithread.h>

#include <assert.h>

rfb::Rect vncDesktop::GetSize()
{
    if (!m_screenCapture)
    {
        m_SWOffsetx = 0;
        m_SWOffsety = 0;
        return rfb::Rect(0, 0, m_scrinfo.framebufferWidth, m_scrinfo.framebufferHeight);
    }
    else
    {
        if (show_all_monitors)
        {
            int nWidth = mymonitor[MULTI_MON_PRIMARY].Width;
            int nHeight = mymonitor[MULTI_MON_PRIMARY].Height;
            switch (nr_monitors)
            {
            case 2:
                nWidth = m_Cliprect.br.x;
                nHeight = m_Cliprect.br.y;
                break;
            case 3: {
                nWidth = mymonitor[MULTI_MON_ALL].Width;
                nHeight = mymonitor[MULTI_MON_ALL].Height;
            }
            default:
                break;
            }
            return rfb::Rect(0, 0, nWidth, nHeight);
        }
        else
        {
            return rfb::Rect(0, 0, mymonitor[static_cast<unsigned long long>(m_current_monitor)].Width, mymonitor[static_cast<unsigned long long>(m_current_monitor)].Height);
        }
    }
}

void vncDesktop::SetBitmapRectOffsetAndClipRect(const int offesetx, const int offsety, int width, int height)
{
    if (width == 0)
    {
        width = mymonitor[MULTI_MON_ALL].Width;
    }
    if (height == 0)
    {
        height = mymonitor[MULTI_MON_ALL].Height;
    }

    if (show_all_monitors)
    {
        m_ScreenOffsetx = mymonitor[MULTI_MON_ALL].offsetx;
        m_ScreenOffsety = mymonitor[MULTI_MON_ALL].offsety;
    }
    else
    {
        m_ScreenOffsetx = mymonitor[MULTI_MON_PRIMARY].offsetx;
        m_ScreenOffsety = mymonitor[MULTI_MON_PRIMARY].offsety;
    }
    m_bmrect = rfb::Rect(offesetx, offsety, width, height);
    m_SWOffsetx = m_bmrect.tl.x;
    m_SWOffsety = m_bmrect.tl.y;
    m_Cliprect.tl.x = 0;
    m_Cliprect.tl.y = 0;
    m_Cliprect.br.x = m_bmrect.br.x;
    m_Cliprect.br.y = m_bmrect.br.y;
    if (m_screenCapture && (m_current_monitor != MULTI_MON_ALL))
    {
        m_SWOffsetx = mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsetx - mymonitor[MULTI_MON_ALL].offsetx;
        m_SWOffsety = mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsety - mymonitor[MULTI_MON_ALL].offsety;

        m_ScreenOffsetx = mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsetx;
        m_ScreenOffsety = mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsety;

        m_Cliprect.tl.x = mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsetx - mymonitor[MULTI_MON_ALL].offsetx;
        m_Cliprect.tl.y = mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsety - mymonitor[MULTI_MON_ALL].offsety;
        m_Cliprect.br.x =
            mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsetx + mymonitor[static_cast<unsigned long long>(m_current_monitor)].Width - mymonitor[MULTI_MON_ALL].offsetx;
        m_Cliprect.br.y =
            mymonitor[static_cast<unsigned long long>(m_current_monitor)].offsety + mymonitor[static_cast<unsigned long long>(m_current_monitor)].Height - mymonitor[MULTI_MON_ALL].offsety;
    }
}
