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

#include "vnclog.h"

#include <winsock2.h>

#include <windows.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <stdio.h>
#include <string.h>

bool G_USE_PIXEL{ false };
extern VNCLog vnclog;
#define VNCLOG(s) (__FUNCTION__ " : " s)

struct _BMInfo
{
    BOOL truecolour;
    BITMAPINFO bmi;
    // Colormap info - comes straight after BITMAPINFO - **HACK**
    std::array<RGBQUAD, 256U> cmap;
} m_bminfo;

void testBench()
{
    HDC m_hrootdc = nullptr;
    HDC m_hmemdc = nullptr;
    HBITMAP m_membitmap = nullptr;
    HBITMAP m_oldbitmap = nullptr;
    void* m_DIBbits = nullptr;

    m_hrootdc = ::GetDC(nullptr);
    if (m_hrootdc == nullptr)
    {
        return;
    }
    m_hmemdc = ::CreateCompatibleDC(m_hrootdc);
    if (m_hmemdc == nullptr)
    {
        return;
    }
    m_membitmap = ::CreateCompatibleBitmap(m_hrootdc, 1, 1);
    if (m_membitmap == nullptr)
    {
        return;
    }


    int result;
    std::memset(&m_bminfo, 0, sizeof(m_bminfo));
    m_bminfo.bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    m_bminfo.bmi.bmiHeader.biBitCount = 0;
    result = ::GetDIBits(m_hmemdc, m_membitmap, 0, 1, nullptr, &m_bminfo.bmi, DIB_RGB_COLORS);
    if (result == 0)
    {
        return;
    }
    result = ::GetDIBits(m_hmemdc, m_membitmap, 0, 1, nullptr, &m_bminfo.bmi, DIB_RGB_COLORS);
    if (result == 0)
    {
        return;
    }
    RECT testRect{};
    testRect.left = 0;
    testRect.top = 0;
    testRect.right = ::GetDeviceCaps(m_hrootdc, HORZRES);
    testRect.bottom = ::GetDeviceCaps(m_hrootdc, VERTRES);
    // Henceforth we want to use a top-down scanning representation
    m_bminfo.bmi.bmiHeader.biWidth = testRect.right;
    m_bminfo.bmi.bmiHeader.biHeight = testRect.bottom;
    m_bminfo.bmi.bmiHeader.biSizeImage =
        std::abs((m_bminfo.bmi.bmiHeader.biWidth * m_bminfo.bmi.bmiHeader.biHeight * m_bminfo.bmi.bmiHeader.biBitCount) / 8);
    m_bminfo.bmi.bmiHeader.biHeight = -std::abs(m_bminfo.bmi.bmiHeader.biHeight);

    // Is the bitmap palette-based or truecolour?
    m_bminfo.truecolour = (::GetDeviceCaps(m_hmemdc, RASTERCAPS) & RC_PALETTE) == 0;

    const int m_bytesPerRow = m_bminfo.bmi.bmiHeader.biWidth * m_bminfo.bmi.bmiHeader.biBitCount / 8;
    int m_bytesPerPixel = m_bminfo.bmi.bmiHeader.biBitCount / 8;

    HBITMAP tempbitmap = ::CreateDIBSection(m_hmemdc, &m_bminfo.bmi, DIB_RGB_COLORS, &m_DIBbits, nullptr, 0);
    if (tempbitmap == nullptr)
    {
        m_DIBbits = nullptr;
        tempbitmap = ::CreateCompatibleBitmap(m_hrootdc, testRect.right, testRect.bottom);
        if (tempbitmap == nullptr)
        {
            return;
        }
    }

    // Delete the old memory bitmap
    if (m_membitmap != nullptr)
    {
        ::DeleteObject(m_membitmap);
        m_membitmap = nullptr;
    }

    // Replace old membitmap with DIB section
    m_membitmap = tempbitmap;
    DWORD time1, time2;
    ///---------------------------------------
    {
        DWORD start = GetTimeFunction();

        {
            if ((m_oldbitmap = (HBITMAP)::SelectObject(m_hmemdc, m_membitmap)) == nullptr)
            {
                return;
            }
            ::BitBlt(m_hmemdc, 0, 0, testRect.right, testRect.bottom, m_hrootdc, 0, 0, CAPTUREBLT | SRCCOPY);
            ::SelectObject(m_hmemdc, m_oldbitmap);
        }
        COLORREF cr{ 0U };
        for (int xx{ 0 }; xx < testRect.right / 32; ++xx)
            for (int yy{ 0 }; yy < testRect.bottom / 32; ++yy)
            {
                unsigned int index = (m_bytesPerRow * yy) + (m_bytesPerPixel * xx);
                std::memcpy(&cr, ((char*)m_DIBbits) + index, m_bytesPerPixel);
            }


        DWORD stop = GetTimeFunction();
        time1 = stop - start;
    }

    ///---------------------------------------
    {
        DWORD start = GetTimeFunction();

        {
            for (int xx{ 0 }; xx < testRect.right * testRect.bottom / 32 / 32 / 200; ++xx)
            {
                ::GetPixel(m_hrootdc, 1, 1);
            }
        }
        DWORD stop = GetTimeFunction();
        time2 = (stop - start) * 200;
    }
    ///---------------------------------------

    if (time2 < time1)
    {
        G_USE_PIXEL = true;
    }
    else
    {
        G_USE_PIXEL = false;
    }

    vnclog.Print(9, VNCLOG("Blit time %i  GetPixelTime %i  Use GetPixel= %i\n"), time1, time2, G_USE_PIXEL);

    if (m_hrootdc != nullptr)
    {
        if (!::ReleaseDC(nullptr, m_hrootdc))
        {
            m_hrootdc = nullptr;
        }
    }
    if (m_hmemdc != nullptr)
    {
        // Release our device context
        if (!::DeleteDC(m_hmemdc)) {}
        m_hmemdc = nullptr;
    }
    if (m_membitmap != nullptr)
    {
        // Release the custom bitmap, if any
        if (!::DeleteObject(m_membitmap)) {}
        m_membitmap = nullptr;
    }
}