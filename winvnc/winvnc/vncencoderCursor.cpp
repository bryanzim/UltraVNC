// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


//  Copyright (C) 2000 Const Kaplinsky. All Rights Reserved.
#include "stdhdrs.h"

#include "vncbuffer.h"
#include "vncdesktop.h"
#include "vncencoder.h"

#include <array>
#include <cstdlib>
#include <cstring>
//
// New code implementing cursor shape updates.
//

BOOL vncEncoder::IsXCursorSupported() { return m_use_xcursor || m_use_richcursor; }

// Cache for accessibility cursor size (avoid reading registry on every cursor update)
static DWORD s_cachedCursorBaseSize{ 0U };
static DWORD s_lastCursorSizeCheck{ 0U };

static DWORD GetAccessibilityCursorSize()
{
    // Cache cursor size for 5 seconds to avoid registry reads on every cursor update
    DWORD now = ::GetTickCount();
    if ((s_cachedCursorBaseSize != 0) && ((now - s_lastCursorSizeCheck) < 5000))
    {
        return s_cachedCursorBaseSize;
    }

    DWORD cursorBaseSize{ 32U }; // Default cursor size
    HKEY hKey;

    // Try primary location: Control Panel\Cursors\CursorBaseSize
    if (::RegOpenKeyEx(HKEY_CURRENT_USER, "Control Panel\\Cursors", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD dataSize = sizeof(DWORD);
        RegQueryValueEx(hKey, "CursorBaseSize", nullptr, nullptr, reinterpret_cast<LPBYTE>(&cursorBaseSize), &dataSize);
        ::RegCloseKey(hKey);
    }

    // Also check Software\Microsoft\Accessibility for CursorSize multiplier (Windows 10/11)
    if (cursorBaseSize == 32)
    {
        if (::RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Accessibility", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
        {
            DWORD cursorSizeMultiplier{ 1U };
            DWORD dataSize = sizeof(DWORD);
            if (RegQueryValueEx(hKey, "CursorSize", nullptr, nullptr, reinterpret_cast<LPBYTE>(&cursorSizeMultiplier), &dataSize) == ERROR_SUCCESS)
            {
                // CursorSize is a multiplier (1-15), convert to pixel size
                if (cursorSizeMultiplier > 1)
                {
                    cursorBaseSize = 32 + (cursorSizeMultiplier - 1) * 8;
                }
            }
            ::RegCloseKey(hKey);
        }
    }

    s_cachedCursorBaseSize = cursorBaseSize;
    s_lastCursorSizeCheck = now;
    return cursorBaseSize;
}

BOOL vncEncoder::SendEmptyCursorShape(VSocket* const outConn)
{
    rfbFramebufferUpdateRectHeader hdr;
    hdr.r.x = Swap16IfLE(0);
    hdr.r.y = Swap16IfLE(0);
    hdr.r.w = Swap16IfLE(0);
    hdr.r.h = Swap16IfLE(0);
    if (m_use_xcursor)
    {
        hdr.encoding = Swap32IfLE(rfbEncodingXCursor);
    }
    else
    {
        hdr.encoding = Swap32IfLE(rfbEncodingRichCursor);
    }

    return outConn->SendExactQueue(reinterpret_cast<char*>(&hdr), sizeof(hdr));
}

static BOOL IsMagentaLikePixel(BYTE* const pixel, const int bytes_pixel)
{
    if (bytes_pixel >= 3)
    {
        int c0 = pixel[0U];
        int c1 = pixel[1U];
        int c2 = pixel[2U];
        return (c1 < 64) && (c0 > 64) && (c2 > 64) && (std::abs(c0 - c2) < 64);
    }
    if (bytes_pixel == 2)
    {
        WORD pixel16 = *reinterpret_cast<WORD*>(pixel);
        return ((pixel16 == 0xF81F) || (pixel16 == 0x7C1F));
    }
    return FALSE;
}

static BOOL HasCursorAlpha(const BYTE* const pixels, const int width, const int height, const int stride, const int bytes_pixel)
{
    if (bytes_pixel != 4)
    {
        return FALSE;
    }
    for (int y{ 0 }; y < height; ++y)
    {
        for (int x{ 0 }; x < width; ++x)
        {
            if (pixels[y * stride + x * 4 + 3] != 0)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static BOOL RestoreAlphaCursorPixel(BYTE* const pixel)
{
    const int alpha = pixel[3U];
    if (alpha == 0)
    {
        std::memset(pixel, 0, 4);
        return FALSE;
    }

    // DrawIconEx blended the cursor over magenta. Remove that
    // background before converting its alpha to RichCursor's one-bit mask.
    // Do not classify these pixels by color: anti-aliased black outlines can
    // look magenta after blending and would otherwise be discarded.
    for (int channel{ 0 }; channel < 3; ++channel)
    {
        int value = pixel[channel] - (channel == 1 ? 0 : 255 - alpha);
        if (value < 0)
        {
            value = 0;
        }
        value = (value * 255 + alpha / 2) / alpha;
        pixel[channel] = static_cast<BYTE>((value > 255 ? 255 : value));
    }
    pixel[3U] = 255;
    return TRUE;
}

BOOL vncEncoder::SendCursorShape(VSocket* const outConn, vncDesktop* const desktop)
{
    // Make sure the function is used correctly
    if (!m_use_xcursor && !m_use_richcursor)
    {
        return FALSE;
    }

    // Check mouse cursor handle
    HCURSOR hcursor = desktop->GetCursor();
    if (hcursor == nullptr)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("cursor handle is NULL.\n"));
        return FALSE;
    }

    // Get cursor info
    ICONINFO IconInfo;
    if (!::GetIconInfo(hcursor, &IconInfo))
    {
        vnclog.Print(LL_INTINFO, VNCLOG("GetIconInfo() failed.\n"));
        return FALSE;
    }
    BOOL isColorCursor = FALSE;
    BITMAP bmColor{ 0 };
    if (IconInfo.hbmColor != nullptr)
    {
        isColorCursor = TRUE;
        // Get color bitmap dimensions before deleting - needed for enlarged accessibility cursors
        ::GetObject(IconInfo.hbmColor, sizeof(BITMAP), reinterpret_cast<LPVOID>(&bmColor));
        ::DeleteObject(IconInfo.hbmColor);
    }
    if (IconInfo.hbmMask == nullptr)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("cursor bitmap handle is NULL.\n"));
        return FALSE;
    }

    // Check bitmap info for the cursor
    BITMAP bmMask;
    if (!::GetObject(IconInfo.hbmMask, sizeof(BITMAP), reinterpret_cast<LPVOID>(&bmMask)))
    {
        vnclog.Print(LL_INTINFO, VNCLOG("GetObject() for bitmap failed.\n"));
        ::DeleteObject(IconInfo.hbmMask);
        return FALSE;
    }
    if ((bmMask.bmPlanes != 1) || (bmMask.bmBitsPixel != 1))
    {
        vnclog.Print(LL_INTINFO, VNCLOG("incorrect data in cursor bitmap.\n"));
        ::DeleteObject(IconInfo.hbmMask);
        return FALSE;
    }

    // Compute cursor dimensions
    // For color cursors (including enlarged accessibility cursors), use color bitmap dimensions
    // For monochrome cursors, mask height is doubled (AND mask + XOR mask)
    int width, height;
    if (isColorCursor && (bmColor.bmWidth > 0) && (bmColor.bmHeight > 0))
    {
        width = bmColor.bmWidth;
        height = bmColor.bmHeight;
    }
    else
    {
        width = bmMask.bmWidth;
        height = (isColorCursor) ? bmMask.bmHeight : bmMask.bmHeight / 2;
    }

    // Check for Windows accessibility cursor scaling (cached to avoid registry reads)
    int xHotspot = static_cast<int>(IconInfo.xHotspot);
    int yHotspot = static_cast<int>(IconInfo.yHotspot);
    DWORD cursorBaseSize = GetAccessibilityCursorSize();

    // If cursor base size is larger than default, scale the cursor
    if ((cursorBaseSize > 32) && (width == 32) && (height == 32))
    {
        width = static_cast<int>(cursorBaseSize);
        height = static_cast<int>(cursorBaseSize);
        // Scale hotspot proportionally
        xHotspot = (IconInfo.xHotspot * cursorBaseSize) / 32;
        yHotspot = (IconInfo.yHotspot * cursorBaseSize) / 32;
    }

    // Get monochrome bitmap data for cursor
    // NOTE: they say we should use GetDIBits() instead of GetBitmapBits().
    BYTE* mbits = new BYTE[static_cast<unsigned long long>(bmMask.bmWidthBytes * bmMask.bmHeight)];
    if (mbits == nullptr)
    {
        ::DeleteObject(IconInfo.hbmMask);
        return FALSE;
    }

    BOOL success = ::GetBitmapBits(IconInfo.hbmMask, bmMask.bmWidthBytes * bmMask.bmHeight, mbits);
    ::DeleteObject(IconInfo.hbmMask);

    if (!success)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("GetBitmapBits() failed.\n"));
        delete[] mbits;
        return FALSE;
    }

    // For enlarged accessibility cursors, the color bitmap may be larger than the mask bitmap.
    // In this case, we need to generate a new mask that matches the color cursor dimensions.
    BYTE* workMbits = mbits;
    int workMaskWidthBytes = bmMask.bmWidthBytes;
    BOOL needsGeneratedMask = isColorCursor && ((width != bmMask.bmWidth) || (height != bmMask.bmHeight));

    if (needsGeneratedMask)
    {
        // Allocate a new mask buffer for the enlarged cursor dimensions
        int newMaskWidthBytes = (width + 7) / 8;
        workMbits = new BYTE[static_cast<unsigned long long>(newMaskWidthBytes * height)];
        if (workMbits == nullptr)
        {
            delete[] mbits;
            return FALSE;
        }
        std::memset(workMbits, 0, static_cast<size_t>(newMaskWidthBytes * height)); // Start with all transparent
        workMaskWidthBytes = newMaskWidthBytes;
    }

    // Call appropriate routine to send cursor shape update
    if (!isColorCursor && m_use_xcursor)
    {
        FixCursorMask(workMbits, nullptr, width, bmMask.bmHeight, workMaskWidthBytes, FALSE);
        success = SendXCursorShape(outConn, workMbits, xHotspot, yHotspot, width, height);
    }
    else if (m_use_richcursor)
    {
        // Calculate cbits size with DWORD-aligned row stride
        int bytes_pixel = m_localformat.bitsPerPixel / 8;
        int cbits_row = width * bytes_pixel;
        while (static_cast<unsigned long long>(cbits_row) % sizeof(DWORD))
        {
            ++cbits_row;
        }
        const int cbits_size = cbits_row * height;
        BYTE* cbits = new BYTE[static_cast<unsigned long long>(cbits_size)];
        if (cbits == nullptr)
        {
            if (needsGeneratedMask)
            {
                delete[] workMbits;
            }
            delete[] mbits;
            return FALSE;
        }
        if (!desktop->GetRichCursorData(cbits, hcursor, width, height, isColorCursor))
        {
            vnclog.Print(LL_INTINFO, VNCLOG("vncDesktop::GetRichCursorData() failed.\n"));
            if (needsGeneratedMask)
            {
                delete[] workMbits;
            }
            delete[] mbits;
            delete[] cbits;
            return FALSE;
        }
        if (needsGeneratedMask)
        {
            // For enlarged accessibility cursors, generate mask from color data
            // Magenta (RGB 255,0,255) is used as transparency key
            // Mask bit 1 = visible pixel, bit 0 = transparent
            const int bytes_pixelInner = m_localformat.bitsPerPixel / 8;
            int bytes_row = width * bytes_pixelInner;
            while (static_cast<unsigned long long>(bytes_row) % sizeof(DWORD))
            {
                ++bytes_row;
            }
            const int packed_width_bytes = workMaskWidthBytes;
            const BOOL hasAlpha = HasCursorAlpha(cbits, width, height, bytes_row, bytes_pixelInner);

            for (int y{ 0 }; y < height; ++y)
            {
                BYTE* srcRow = cbits + y * bytes_row;
                BYTE* dstRow = workMbits + y * packed_width_bytes;
                BYTE bitmask{ 0x80U };
                int dstByteIdx{ 0 };

                for (int x{ 0 }; x < width; ++x)
                {
                    BYTE* pixel = srcRow + x * bytes_pixelInner;
                    BOOL isMagenta = FALSE;

                    // Check if pixel is magenta (transparency key)
                    if (bytes_pixelInner == 4)
                    {
                        // 32-bit BGRA: B=255, G=0, R=255
                        isMagenta = ((pixel[0U] == 255) && (pixel[1U] == 0) && (pixel[2U] == 255));
                    }
                    else if (bytes_pixelInner == 3)
                    {
                        isMagenta = ((pixel[0U] == 255) && (pixel[1U] == 0) && (pixel[2U] == 255));
                    }
                    else if (bytes_pixelInner == 2)
                    {
                        const WORD pixel16 = *reinterpret_cast<WORD*>(pixel);
                        isMagenta = ((pixel16 == 0xF81F) || (pixel16 == 0x7C1F));
                    }

                    const BOOL visible = hasAlpha ? RestoreAlphaCursorPixel(pixel) : !isMagenta;
                    if (visible)
                    {
                        // Visible pixel - set mask bit
                        dstRow[dstByteIdx] |= bitmask;
                    }
                    else
                    {
                        // Transparent - clear color to black
                        std::memset(pixel, 0, static_cast<size_t>(bytes_pixelInner));
                    }
                    if ((bitmask >>= 1) == 0)
                    {
                        bitmask = 0x80;
                        ++dstByteIdx;
                    }
                }
            }
        }
        else
        {
            FixCursorMask(workMbits, cbits, width, height, workMaskWidthBytes, isColorCursor);
        }
        success = SendRichCursorShape(outConn, workMbits, cbits, xHotspot, yHotspot, width, height);
        delete[] cbits;
    }
    else
    {
        success = FALSE; // FIXME: We could convert RichCursor -> XCursor.
    }

    // Cleanup
    if (needsGeneratedMask)
    {
        delete[] workMbits;
    }
    delete[] mbits;

    return success;
}

BOOL vncEncoder::SendXCursorShape(VSocket* const outConn, BYTE* mask, const int xhot, const int yhot, const int width,
                                  const int height)
{
    rfbFramebufferUpdateRectHeader hdr;
    hdr.r.x = Swap16IfLE(xhot);
    hdr.r.y = Swap16IfLE(yhot);
    hdr.r.w = Swap16IfLE(width);
    hdr.r.h = Swap16IfLE(height);
    hdr.encoding = Swap32IfLE(rfbEncodingXCursor);

    std::array<BYTE, 6U> colors{ 0, 0, 0, 0xFF, 0xFF, 0xFF };
    const int maskRowSize = (width + 7) / 8;
    int maskSize = maskRowSize * height;

    if (!outConn->SendExactQueue(reinterpret_cast<char*>(&hdr), sizeof(hdr)) || !outConn->SendExactQueue(reinterpret_cast<char*>(colors.data()), 6) ||
        !outConn->SendExactQueue(reinterpret_cast<char*>(&mask[maskSize]), static_cast<VCard>(maskSize)) || !outConn->SendExactQueue(reinterpret_cast<char*>(mask), static_cast<VCard>(maskSize)))
    {
        return FALSE;
    }
    return TRUE;
}

BOOL vncEncoder::SendRichCursorShape(VSocket* const outConn, BYTE* const mbits, BYTE* const cbits, const int xhot, const int yhot,
                                     const int width, const int height)
{
    rfbFramebufferUpdateRectHeader hdr;
    hdr.r.x = Swap16IfLE(xhot);
    hdr.r.y = Swap16IfLE(yhot);
    hdr.r.w = Swap16IfLE(width);
    hdr.r.h = Swap16IfLE(height);
    hdr.encoding = Swap32IfLE(rfbEncodingRichCursor);

    // Cet cursor image in local pixel format
    int srcbuf_rowsize = width * (m_localformat.bitsPerPixel / 8);
    while (static_cast<unsigned long long>(srcbuf_rowsize) % sizeof(DWORD))
    {
        ++srcbuf_rowsize;
    } // Actually, this should never happen

    // Translate image to client pixel format
    int dstbuf_size = width * height * (m_remoteformat.bitsPerPixel / 8);
    BYTE* dstbuf = new BYTE[static_cast<unsigned long long>(dstbuf_size)];
    Translate(cbits, dstbuf, width, height, srcbuf_rowsize);

    // Send the data
    const int mask_rowsize = (width + 7) / 8;
    int mask_size = mask_rowsize * height;
    if (!outConn->SendExactQueue(reinterpret_cast<char*>(&hdr), sizeof(hdr)) || !outConn->SendExactQueue(reinterpret_cast<char*>(dstbuf), static_cast<VCard>(dstbuf_size)) ||
        !outConn->SendExactQueue(reinterpret_cast<char*>(mbits), static_cast<VCard>(mask_size)))
    {
        delete[] dstbuf;
        return FALSE;
    }
    delete[] dstbuf;
    return TRUE;
}

void vncEncoder::FixCursorMask(BYTE* const mbits, BYTE* cbits, const int width, int height, int width_bytes,
                               const BOOL isColorCursor)
{
    int packed_width_bytes = (width + 7) / 8;

    // Pack and invert bitmap data (mbits)
    // IMPORTANT: Process rows from end to beginning when packing in-place
    // to avoid overwriting data we haven't read yet (when packed_width_bytes < width_bytes)
    int x, y;
    for (y = height - 1; y >= 0; --y)
    {
        for (x = packed_width_bytes - 1; x >= 0; --x)
        {
            mbits[y * packed_width_bytes + x] = ~mbits[y * width_bytes + x];
        }
    }

    // Replace "inverted background" bits with black color to ensure
    // cross-platform interoperability. Not beautiful but necessary code.
    if (cbits == nullptr)
    {
        BYTE m, c;
        height /= 2;
        for (y = 0; y < height; ++y)
        {
            for (x = 0; x < packed_width_bytes; ++x)
            {
                m = mbits[y * packed_width_bytes + x];
                c = mbits[(height + y) * packed_width_bytes + x];
                mbits[y * packed_width_bytes + x] |= ~(m | c);
                mbits[(height + y) * packed_width_bytes + x] |= ~(m | c);
            }
        }
    }
    else if (isColorCursor)
    {
        // Generate the mask from the color buffer. Pixels close to the magenta
        // background (including its anti-aliased one-pixel shadow ring) are
        // treated as transparent; this removes the purple halo on cursors such
        // as the hand, Busy and Working in Background pointers.
        int bytes_pixel = m_localformat.bitsPerPixel / 8;
        int bytes_row = width * bytes_pixel;
        while (static_cast<unsigned long long>(bytes_row) % sizeof(DWORD))
        {
            ++bytes_row;
        }
        const BOOL hasAlpha = HasCursorAlpha(cbits, width, height, bytes_row, bytes_pixel);

        BYTE bitmask;
        for (y = 0; y < height; ++y)
        {
            bitmask = 0x80;
            for (x = 0; x < width; ++x)
            {
                BYTE* pixel = &cbits[y * bytes_row + x * bytes_pixel];

                const BOOL visible = hasAlpha ? RestoreAlphaCursorPixel(pixel) : !IsMagentaLikePixel(pixel, bytes_pixel);
                if (!visible)
                {
                    // Transparent - clear mask bit and set color to black
                    mbits[y * packed_width_bytes + x / 8] &= ~bitmask;
                    std::memset(pixel, 0, static_cast<size_t>(bytes_pixel));
                }
                else
                {
                    // Visible pixel - set mask bit
                    mbits[y * packed_width_bytes + x / 8] |= bitmask;
                }

                if ((bitmask >>= 1) == 0)
                {
                    bitmask = 0x80;
                }
            }
        }
    }
    else
    {
        // For monochrome cursors rendered as RichCursor: use original mask-based logic
        // The mask is already inverted above, just need to handle XOR pixels
        int bytes_pixel = m_localformat.bitsPerPixel / 8;
        int bytes_row = width * bytes_pixel;
        while (static_cast<unsigned long long>(bytes_row) % sizeof(DWORD))
        {
            ++bytes_row;
        }

        BYTE bitmask;
        int b1, b2;
        for (y = 0; y < height; ++y)
        {
            bitmask = 0x80;
            for (x = 0; x < width; ++x)
            {
                if ((mbits[y * packed_width_bytes + x / 8] & bitmask) == 0)
                {
                    for (b1 = 0; b1 < bytes_pixel; ++b1)
                    {
                        if (cbits[y * bytes_row + x * bytes_pixel + b1] != 0)
                        {
                            mbits[y * packed_width_bytes + x / 8] ^= bitmask;
                            for (b2 = b1; b2 < bytes_pixel; ++b2)
                            {
                                cbits[y * bytes_row + x * bytes_pixel + b2] = 0x00;
                            }
                            break;
                        }
                    }
                }
                if ((bitmask >>= 1) == 0)
                {
                    bitmask = 0x80;
                }
            }
        }
    }
}

// Translate a rectangle (using arbitrary m_bytesPerRow value,
// always translating from the beginning of the source pixel array)
// NOTE: overloaded function!
inline void vncEncoder::Translate(BYTE* const source, BYTE* const dest, const int w, const int h, const int bytesPerRow)
{
    // Call the translation function
    (*m_transfunc)(m_transtable, &m_localformat, &m_transformat, reinterpret_cast<char*>(source), reinterpret_cast<char*>(dest), bytesPerRow, w, h);
}
