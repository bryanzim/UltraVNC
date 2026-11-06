// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999 AT&T Laboratories Cambridge. All Rights Reserved.
//


// RRE (Rising Rectangle Encoding)
//
// The bits of the ClientConnection object to do with RRE.

#include "stdhdrs.h"

#include "ClientConnection.h"
#include "Exception.h"
#include "vncviewer.h"

#include <array>

extern std::array<wchar_t, 64U> sz_L70;

void ClientConnection::ReadRRERect(rfbFramebufferUpdateRectHeader* const pfburh)
{
    // An RRE rect is always followed by a background color
    // For speed's sake we read them together into a buffer.
    char tmpbuf[sz_rfbRREHeader + 4]; // biggest pixel is 4 bytes long
    rfbRREHeader* prreh = reinterpret_cast<rfbRREHeader*>(tmpbuf);
    CARD8* pcolor = reinterpret_cast<CARD8*>(tmpbuf) + sz_rfbRREHeader;
    ReadExact(tmpbuf, static_cast<int>(sz_rfbRREHeader + m_minPixelBytes));

    prreh->nSubrects = Swap32IfLE(prreh->nSubrects);

    // No other threads can use bitmap DC
    omni_mutex_lock l(m_bitmapdcMutex);

    // Draw the background of the rectangle
    FillSolidRect_ultra(pfburh->r.x, pfburh->r.y, pfburh->r.w, pfburh->r.h, m_myFormat.bitsPerPixel, pcolor);

    if (prreh->nSubrects == 0)
    {
        return;
    }
    if (prreh->nSubrects > 20000000)
    {
        throw ErrorException(sz_L70.data());
    }

    // Draw the sub-rectangles
    rfbRectangle rect, *pRect;
    // The size of an RRE subrect including color info
    int subRectSize = static_cast<int>(m_minPixelBytes + sz_rfbRectangle);

    // Read subrects into the buffer
    CheckBufferSize(static_cast<CARD32>(subRectSize) * prreh->nSubrects);
    ReadExact(m_netbuf, static_cast<int>(subRectSize * prreh->nSubrects));
    BYTE* p = reinterpret_cast<BYTE*>(m_netbuf);
    for (CARD32 i = 0; i < prreh->nSubrects; ++i)
    {
        pRect = reinterpret_cast<rfbRectangle*>((p + m_minPixelBytes));

        rect.x = static_cast<CARD16>((Swap16IfLE(pRect->x) + pfburh->r.x));
        rect.y = static_cast<CARD16>((Swap16IfLE(pRect->y) + pfburh->r.y));
        rect.w = Swap16IfLE(pRect->w);
        rect.h = Swap16IfLE(pRect->h);

        FillSolidRect_ultra(rect.x, rect.y, rect.w, rect.h, m_myFormat.bitsPerPixel, p);
        p += subRectSize;
    }
}
