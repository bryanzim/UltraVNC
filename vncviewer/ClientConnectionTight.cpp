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

#include "ClientConnection.h"
#include "Exception.h"
#include "ViewerJpegSource.h"
#include "vncviewer.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>

#define TIGHT_MIN_TO_COMPRESS 12
#define TIGHT_BUFFER_SIZE (2048 * 200)

void ClientConnection::ReadTightRect(rfbFramebufferUpdateRectHeader* const pfburh, const bool zstd)
{
    if ((m_myFormat.bitsPerPixel != 8) && (m_myFormat.bitsPerPixel != 16) && (m_myFormat.bitsPerPixel != 32))
    {
        vnclog.Print(0, _T("Invalid number of bits per pixel: %d\n"), m_myFormat.bitsPerPixel);
        return;
    }

    // Hardening: reject malformed rectangles before the gradient/palette
    // filters can overflow their fixed-size buffers.
    if ((static_cast<int>(pfburh->r.x) + static_cast<int>(pfburh->r.w) > static_cast<int>(m_si.framebufferWidth)) ||
        (static_cast<int>(pfburh->r.y) + static_cast<int>(pfburh->r.h) > static_cast<int>(m_si.framebufferHeight)))
    {
        vnclog.Print(0, _T("Tight encoding: invalid rectangle %dx%d at (%d,%d), framebuffer %dx%d\n"), pfburh->r.w, pfburh->r.h,
                     pfburh->r.x, pfburh->r.y, m_si.framebufferWidth, m_si.framebufferHeight);
        throw WarningException(L"Invalid Tight rectangle received from server.");
    }

    CARD8 comp_ctl;
    ReadExact(reinterpret_cast<char*>(&comp_ctl), 1);

    /* Flush zlib streams if we are told by the server to do so. */
    for (int i{ 0 }; i < 4; ++i)
    {
        if (comp_ctl & 1)
        {
            UltraVncZ* uz = &ultraVncZTight[static_cast<unsigned long long>(i)];
            uz->endInflateStream(zstd);
        }
        comp_ctl >>= 1;
    }

    /* Handle solid rectangles. */
    std::array<BYTE, 4U> colorpointer;
    if (comp_ctl == rfbTightFill)
    {
        if ((m_myFormat.depth == 24) && (m_myFormat.redMax == 0xFF) && (m_myFormat.greenMax == 0xFF) &&
            (m_myFormat.blueMax == 0xFF))
        {
            std::array<CARD8, 3U> fillColourBuf;
            ReadExact(reinterpret_cast<char*>(fillColourBuf.data()), 3);
            CARD32* p = reinterpret_cast<CARD32*>(colorpointer.data());
            *p = (static_cast<CARD32>(fillColourBuf[0U]) << m_myFormat.redShift) | (static_cast<CARD32>(fillColourBuf[1U]) << m_myFormat.greenShift) |
                 (static_cast<CARD32>(fillColourBuf[2U]) << m_myFormat.blueShift);
        }
        else
        {
            CARD32 fillColourBuf;
            ReadExact(reinterpret_cast<char*>(&fillColourBuf), m_myFormat.bitsPerPixel / 8);
            std::memcpy(colorpointer.data(), &fillColourBuf, m_myFormat.bitsPerPixel / 8);
        }

        omni_mutex_lock l(m_bitmapdcMutex);

        FillSolidRect_ultra(pfburh->r.x, pfburh->r.y, pfburh->r.w, pfburh->r.h, m_myFormat.bitsPerPixel,
                            reinterpret_cast<BYTE*>(colorpointer.data()));
        return;
    }

    if (comp_ctl == rfbTightJpeg)
    {
        DecompressJpegRect(pfburh->r.x, pfburh->r.y, pfburh->r.w, pfburh->r.h);
        return;
    }


    /* Quit on unsupported subencoding value. */
    if (comp_ctl >= rfbTightMaxSubencoding)
    {
        vnclog.Print(0, _T("Tight encoding: bad subencoding value received.\n"));
        return;
    }

    if (pfburh->r.w > sizeof(m_tightPrevRow) / (3 * sizeof(CARD16)))
    {
        vnclog.Print(0, _T("Tight encoding: rectangle width %d exceeds filter limit\n"), pfburh->r.w);
        throw WarningException(L"Invalid Tight rectangle received from server.");
    }

    /*
     * Here primary compression mode handling begins.
     * Data was processed with optional filter + Zlib compression.
     */

    /* First, we should identify a filter to use. */
    int bitsPixel;
    if ((comp_ctl & rfbTightExplicitFilter) != 0)
    {
        CARD8 filter_id;
        ReadExact(reinterpret_cast<char*>(&filter_id), 1);

        switch (filter_id)
        {
        case rfbTightFilterCopy:
            bitsPixel = InitFilterCopy(pfburh->r.w, pfburh->r.h);
            break;
        case rfbTightFilterPalette:
            bitsPixel = InitFilterPalette(pfburh->r.w, pfburh->r.h);
            break;
        case rfbTightFilterGradient:
            bitsPixel = InitFilterGradient(pfburh->r.w, pfburh->r.h);
            break;
        default:
            vnclog.Print(0, _T("Tight encoding: unknown filter code received.\n"));
            return;
        }
    }
    else
    {
        bitsPixel = InitFilterCopy(pfburh->r.w, pfburh->r.h);
    }
    if (bitsPixel == 0)
    {
        vnclog.Print(0, _T("Tight encoding: error receiving palette.\n"));
        return;
    }

    /* Determine if the data should be decompressed or just copied. */
    int rowSize = (pfburh->r.w * bitsPixel + 7) / 8;
    if (pfburh->r.h * rowSize < TIGHT_MIN_TO_COMPRESS)
    {
        CheckBufferSize(static_cast<UINT>(pfburh->r.h * rowSize));
        ReadExact(m_netbuf, pfburh->r.h * rowSize);

        CheckZlibBufferSize(pfburh->r.w * pfburh->r.h * 4);
        (this->*m_tightCurrentFilter)(pfburh->r.h);

        omni_mutex_lock l(m_bitmapdcMutex);
        SETPIXELS_NOCONV(m_zlibbuf, pfburh->r.x, pfburh->r.y, pfburh->r.w, pfburh->r.h)

        return;
    }

    /* Read the length (1..3 bytes) of compressed data following. */

    int compressedLen = ReadCompactLen();
    if (compressedLen <= 0)
    {
        vnclog.Print(0, _T("Tight encoding: bad data received from server.\n"));
        return;
    }

    /* Now let's initialize compression stream if needed. */
    const int stream_id = comp_ctl & 0x03;
    UltraVncZ* uz = &ultraVncZTight[static_cast<unsigned long long>(stream_id)];

    /* Read, decode and draw actual pixel data in a loop. */
    int beforeBufferSize = static_cast<int>(TIGHT_BUFFER_SIZE * bitsPixel / (bitsPixel + sizeof(COLORREF) * 8) & 0xFFFFFFFC);
    CheckBufferSize(static_cast<UINT>(beforeBufferSize));

    int afterBufferSize = TIGHT_BUFFER_SIZE - beforeBufferSize;
    CheckZlibBufferSize(afterBufferSize);

    int rowsProcessed{ 0 }, extraBytes = 0;
    int err, numRows, portionLen;

    while (compressedLen > 0)
    {
        if (compressedLen > TIGHT_ZLIB_BUFFER_SIZE)
        {
            portionLen = TIGHT_ZLIB_BUFFER_SIZE;
        }
        else
        {
            portionLen = compressedLen;
        }

        ReadExact(m_tightbuf.data(), portionLen);

        compressedLen -= portionLen;

        BYTE* next_in = reinterpret_cast<Bytef*>(m_tightbuf.data());
        UINT avail_in = static_cast<UINT>(portionLen);
        BYTE* next_out;
        UINT avail_out;

        do
        {
            next_out = reinterpret_cast<Bytef*>(&m_netbuf[extraBytes]);
            avail_out = static_cast<UINT>(beforeBufferSize - extraBytes);

            err = uz->decompress(avail_in, avail_out, next_in, next_out, zstd);

            if (err != Z_OK)
            { // Input exhausted -- no problem
                return;
            }

            numRows = static_cast<int>((beforeBufferSize - avail_out) / rowSize);

            (this->*m_tightCurrentFilter)(numRows);

            extraBytes = static_cast<UINT>(beforeBufferSize) - static_cast<int>(avail_out - numRows * rowSize);
            if (extraBytes > 0)
            {
                std::memcpy(m_netbuf, &m_netbuf[numRows * rowSize], static_cast<size_t>(extraBytes));
            }

            omni_mutex_lock l(m_bitmapdcMutex);

            SETPIXELS_NOCONV(m_zlibbuf, pfburh->r.x, pfburh->r.y + rowsProcessed, pfburh->r.w, numRows)

            rowsProcessed += numRows;
        } while (avail_out == 0);
    }

    if (rowsProcessed != pfburh->r.h)
    {
        vnclog.Print(0, _T("Tight encoding: wrong number of scan lines.\n"));
    }
}

int ClientConnection::ReadCompactLen()
{
    CARD8 len_byte;
    ReadExact(reinterpret_cast<char*>(&len_byte), 1);
    int compressedLen = static_cast<int>(len_byte) & 0x7F;
    if (len_byte & 0x80)
    {
        ReadExact(reinterpret_cast<char*>(&len_byte), 1);
        compressedLen |= (static_cast<int>(len_byte) & 0x7F) << 7;
        if (len_byte & 0x80)
        {
            ReadExact(reinterpret_cast<char*>(&len_byte), 1);
            compressedLen |= (static_cast<int>(len_byte) & 0xFF) << 14;
        }
    }
    return compressedLen;
}

//----------------------------------------------------------------------------
//
// Filter stuff.
//

// The following variables are defined in the class declaration:
//   tightFilterFunc m_tightCurrentFilter;
//   Bool m_tightCutZeros;
//   int m_tightRectWidth, m_tightRectColors;
//   COLORREF m_tightPalette[256];
//   CARD8 m_tightPrevRow[2048*3*sizeof(CARD16)];

int ClientConnection::InitFilterCopy(const int rw, [[maybe_unused]] const int rh)
{
    std::array<tightFilterFunc, 3U> funcArray{ &ClientConnection::FilterCopy8, &ClientConnection::FilterCopy16,
                                               &ClientConnection::FilterCopy32 };

    m_tightCurrentFilter = funcArray[m_myFormat.bitsPerPixel / 16];
    m_tightRectWidth = rw;

    if ((m_myFormat.depth == 24) && (m_myFormat.redMax == 0xFF) && (m_myFormat.greenMax == 0xFF) && (m_myFormat.blueMax == 0xFF))
    {
        m_tightCutZeros = TRUE;
        m_tightCurrentFilter = &ClientConnection::FilterCopy24;
        return 24;
    }

    m_tightCutZeros = FALSE;
    return m_myFormat.bitsPerPixel;
}

int ClientConnection::InitFilterGradient(const int rw, const int rh)
{
    const int bits = InitFilterCopy(rw, rh);

    std::array<tightFilterFunc, 3U> funcArray{ &ClientConnection::FilterGradient8, &ClientConnection::FilterGradient16,
                                               &ClientConnection::FilterGradient32 };

    m_tightCurrentFilter = funcArray[m_myFormat.bitsPerPixel / 16];

    if (m_tightCutZeros)
    {
        m_tightCurrentFilter = &ClientConnection::FilterGradient24;
        std::memset(m_tightPrevRow.data(), 0, static_cast<size_t>(rw * 3));
    }
    else
    {
        std::memset(m_tightPrevRow.data(), 0, static_cast<unsigned long long>(rw * 3) * sizeof(CARD16));
    }

    return bits;
}

int ClientConnection::InitFilterPalette(const int rw, [[maybe_unused]] const int rh)
{
    m_tightCurrentFilter = &ClientConnection::FilterPalette;
    m_tightRectWidth = rw;

    CARD8 numColors;
    ReadExact(reinterpret_cast<char*>(&numColors), 1);

    m_tightRectColors = static_cast<int>(numColors);
    if (++m_tightRectColors < 2)
    {
        return 0;
    }

    if ((m_myFormat.depth == 24) && (m_myFormat.redMax == 0xFF) && (m_myFormat.greenMax == 0xFF) && (m_myFormat.blueMax == 0xFF))
    {

        CheckBufferSize(static_cast<UINT>(m_tightRectColors * 3));
        ReadExact(m_netbuf, m_tightRectColors * 3);

        for (int i{ 0 }; i < m_tightRectColors; ++i)
        {
            m_tightPalette[static_cast<unsigned long long>(i)] = COLOR_FROM_PIXEL24_ADDRESS(&m_netbuf[i * 3]);
        }
    }
    else
    {
        CheckBufferSize(static_cast<UINT>(m_tightRectColors * (m_myFormat.bitsPerPixel / 8)));
        ReadExact(m_netbuf, m_tightRectColors * (m_myFormat.bitsPerPixel / 8));

        SETUP_COLOR_SHORTCUTS

        int i;
        switch (m_myFormat.bitsPerPixel)
        {
        case 8:
            for (i = 0; i < m_tightRectColors; ++i)
            {
                m_tightPalette[static_cast<unsigned long long>(i)] = COLOR_FROM_PIXEL8_ADDRESS(&m_netbuf[i]);
            }
            break;
        case 16:
            for (i = 0; i < m_tightRectColors; ++i)
            {
                m_tightPalette[static_cast<unsigned long long>(i)] = COLOR_FROM_PIXEL16_ADDRESS(&m_netbuf[i * 2]);
            }
            break;
        default:
            for (i = 0; i < m_tightRectColors; ++i)
            {
                m_tightPalette[static_cast<unsigned long long>(i)] = COLOR_FROM_PIXEL32_ADDRESS(&m_netbuf[i * 4]);
            }
        }
    }

    return (m_tightRectColors == 2) ? 1 : 8;
}

//
// Actual filtering code follows.
//

#define DEFINE_TIGHT_FILTER_COPY(bpp)                                                                                              \
                                                                                                                                   \
    void ClientConnection::FilterCopy##bpp(int numRows)                                                                            \
    {                                                                                                                              \
        COLORREF* dst = reinterpret_cast<COLORREF*>(m_zlibbuf);                                                                                      \
                                                                                                                                   \
        SETUP_COLOR_SHORTCUTS;                                                                                                     \
                                                                                                                                   \
        int x;                                                                                                                     \
        for (int y{ 0 }; y < numRows; ++y)                                                                                         \
        {                                                                                                                          \
            for (x = 0; x < m_tightRectWidth; ++x)                                                                                 \
            {                                                                                                                      \
                dst[y * m_tightRectWidth + x] =                                                                                    \
                    COLOR_FROM_PIXEL##bpp##_ADDRESS(m_netbuf + (y * m_tightRectWidth + x) * (bpp / 8));                            \
            }                                                                                                                      \
        }                                                                                                                          \
    }

DEFINE_TIGHT_FILTER_COPY(8)
DEFINE_TIGHT_FILTER_COPY(16)
DEFINE_TIGHT_FILTER_COPY(24)
DEFINE_TIGHT_FILTER_COPY(32)

#define DEFINE_TIGHT_FILTER_GRADIENT(bpp)                                                                                          \
                                                                                                                                   \
    void ClientConnection::FilterGradient##bpp(int numRows)                                                                        \
    {                                                                                                                              \
        int x, y, c;                                                                                                               \
        CARD##bpp* src = (CARD##bpp*)m_netbuf;                                                                                     \
        COLORREF* dst = reinterpret_cast<COLORREF*>(m_zlibbuf);                                                                                      \
        CARD16* thatRow = reinterpret_cast<CARD16*>(m_tightPrevRow.data());                                                                                 \
        std::array<CARD16, 2048 * 3> thisRow;                                                                                                  \
        CARD16 pix[3U];                                                                                                            \
        CARD16 max[3U];                                                                                                            \
        int shift[3U];                                                                                                             \
        std::array<int, 3U> est;                                                                                                               \
                                                                                                                                   \
        max[0U] = m_myFormat.redMax;                                                                                               \
        max[1U] = m_myFormat.greenMax;                                                                                             \
        max[2U] = m_myFormat.blueMax;                                                                                              \
                                                                                                                                   \
        shift[0U] = m_myFormat.redShift;                                                                                           \
        shift[1U] = m_myFormat.greenShift;                                                                                         \
        shift[2U] = m_myFormat.blueShift;                                                                                          \
                                                                                                                                   \
        for (y = 0; y < numRows; ++y)                                                                                              \
        {                                                                                                                          \
                                                                                                                                   \
            /* First pixel in a row */                                                                                             \
            for (c = 0; c < 3; ++c)                                                                                                \
            {                                                                                                                      \
                pix[c] = static_cast<CARD16>(((src[y * m_tightRectWidth] >> shift[c]) + thatRow[c] & max[c]));                                  \
                thisRow[c] = pix[c];                                                                                               \
            }                                                                                                                      \
            dst[y * m_tightRectWidth] =                                                                                            \
                PALETTERGB(static_cast<CARD32>(pix[0U]) * 255 / max[0U], static_cast<CARD32>(pix[1U]) * 255 / max[1U], static_cast<CARD32>(pix[2U]) * 255 / max[2U]);     \
                                                                                                                                   \
            /* Remaining pixels of a row */                                                                                        \
            for (x = 1; x < m_tightRectWidth; ++x)                                                                                 \
            {                                                                                                                      \
                for (c = 0; c < 3; ++c)                                                                                            \
                {                                                                                                                  \
                    est[c] = static_cast<int>(thatRow[x * 3 + c]) + static_cast<int>(pix[c]) -                                     \
                             static_cast<int>(thatRow[(x - 1) * 3 + c]);                                                           \
                    if (est[c] > static_cast<int>(max[c]))                                                                         \
                    {                                                                                                              \
                        est[c] = static_cast<int>(max[c]);                                                                         \
                    }                                                                                                              \
                    else if (est[c] < 0)                                                                                           \
                    {                                                                                                              \
                        est[c] = 0;                                                                                                \
                    }                                                                                                              \
                    pix[c] = static_cast<CARD16>(((src[y * m_tightRectWidth + x] >> shift[c]) + est[c] & max[c]));                              \
                    thisRow[x * 3 + c] = pix[c];                                                                                   \
                }                                                                                                                  \
                dst[y * m_tightRectWidth + x] =                                                                                    \
                    PALETTERGB(static_cast<CARD32>(pix[0U]) * 255 / max[0U], static_cast<CARD32>(pix[1U]) * 255 / max[1U], static_cast<CARD32>(pix[2U]) * 255 / max[2U]); \
            }                                                                                                                      \
            std::memcpy(thatRow, thisRow.data(), m_tightRectWidth * 3 * sizeof(CARD16));                                                  \
        }                                                                                                                          \
    }

DEFINE_TIGHT_FILTER_GRADIENT(8)
DEFINE_TIGHT_FILTER_GRADIENT(16)
DEFINE_TIGHT_FILTER_GRADIENT(32)

void ClientConnection::FilterGradient24(const int numRows)
{
    std::array<CARD8, 2048 * 3> thisRow;
    std::array<CARD8, 3U> pix;
    std::array<int, 3U> est;

    COLORREF* dst = reinterpret_cast<COLORREF*>(m_zlibbuf);

    for (int y{ 0 }; y < numRows; ++y)
    {

        // First pixel in a row
        for (int c{ 0 }; c < 3; ++c)
        {
            pix[static_cast<unsigned long long>(c)] = m_tightPrevRow[static_cast<unsigned long long>(c)] + m_netbuf[y * m_tightRectWidth * 3 + c];
            thisRow[static_cast<unsigned long long>(c)] = pix[static_cast<unsigned long long>(c)];
        }
        dst[y * m_tightRectWidth] = COLOR_FROM_PIXEL24_ADDRESS(pix.data());

        // Remaining pixels of a row
        for (int x{ 1 }; x < m_tightRectWidth; ++x)
        {
            for (int c{ 0 }; c < 3; ++c)
            {
                est[static_cast<unsigned long long>(c)] = static_cast<int>(m_tightPrevRow[static_cast<unsigned long long>(x * 3 + c)]) + static_cast<int>(pix[static_cast<unsigned long long>(c)]) -
                         static_cast<int>(m_tightPrevRow[static_cast<unsigned long long>((x - 1) * 3 + c)]);
                if (est[static_cast<unsigned long long>(c)] > 0xFF)
                {
                    est[static_cast<unsigned long long>(c)] = 0xFF;
                }
                else if (est[static_cast<unsigned long long>(c)] < 0x00)
                {
                    est[static_cast<unsigned long long>(c)] = 0x00;
                }
                pix[static_cast<unsigned long long>(c)] = static_cast<CARD8>(est[c]) + m_netbuf[(y * m_tightRectWidth + x) * 3 + c];
                thisRow[static_cast<unsigned long long>(x * 3 + c)] = pix[static_cast<unsigned long long>(c)];
            }
            dst[y * m_tightRectWidth + x] = COLOR_FROM_PIXEL24_ADDRESS(pix.data());
        }

        std::memcpy(m_tightPrevRow.data(), thisRow.data(), static_cast<size_t>(m_tightRectWidth * 3));
    }
}

void ClientConnection::FilterPalette(const int numRows)
{
    int x, y, b, w;
    CARD8* src = reinterpret_cast<CARD8*>(m_netbuf);
    COLORREF* dst = reinterpret_cast<COLORREF*>(m_zlibbuf);

    if (m_tightRectColors == 2)
    {
        w = (m_tightRectWidth + 7) / 8;
        for (y = 0; y < numRows; ++y)
        {
            for (x = 0; x < m_tightRectWidth / 8; ++x)
            {
                for (b = 7; b >= 0; --b)
                {
                    dst[y * m_tightRectWidth + x * 8 + 7 - b] = m_tightPalette[src[y * w + x] >> b & 1];
                }
            }
            for (b = 7; b >= 8 - m_tightRectWidth % 8; --b)
            {
                dst[y * m_tightRectWidth + x * 8 + 7 - b] = m_tightPalette[src[y * w + x] >> b & 1];
            }
        }
    }
    else
    {
        for (y = 0; y < numRows; ++y)
        {
            for (x = 0; x < m_tightRectWidth; ++x)
            {
                dst[y * m_tightRectWidth + x] = m_tightPalette[static_cast<int>(src[y * m_tightRectWidth + x])];
            }
        }
    }
}

//
// JPEG decompression code.
//


//
// A "Source manager" for the JPEG library.
//



static void JpegInitSource(j_decompress_ptr cinfo);
static boolean JpegFillInputBuffer(j_decompress_ptr cinfo);
static void JpegSkipInputData(j_decompress_ptr cinfo, long num_bytes);
static void JpegTermSource(j_decompress_ptr cinfo);

static void JpegInitSource(const j_decompress_ptr cinfo) { ViewerJpeg::initSource(cinfo); }

static boolean JpegFillInputBuffer(const j_decompress_ptr cinfo) { return ViewerJpeg::fillInputBuffer(cinfo); }

static void JpegSkipInputData(const j_decompress_ptr cinfo, const long num_bytes) { ViewerJpeg::skipInputData(cinfo, num_bytes); }

static void JpegTermSource([[maybe_unused]] const j_decompress_ptr cinfo) { /* No work necessary here. */ }

/*static void
JpegSetSrcManager(j_decompress_ptr cinfo, char *compressedData, int compressedLen)
{
jpegBufferPtr = (JOCTET *)compressedData;
jpegBufferLen = (size_t)compressedLen;

jpegSrcManager.init_source = JpegInitSource;
jpegSrcManager.fill_input_buffer = JpegFillInputBuffer;
jpegSrcManager.skip_input_data = JpegSkipInputData;
jpegSrcManager.resync_to_restart = jpeg_resync_to_restart;
jpegSrcManager.term_source = JpegTermSource;
jpegSrcManager.next_input_byte = jpegBufferPtr;
jpegSrcManager.bytes_in_buffer = jpegBufferLen;

cinfo->src = &jpegSrcManager;
}*/


/*static void JpegSetSrcManager(j_decompress_ptr cinfo, char *compressedData,
                              int compressedLen);*/
void JpegErrorHeader(const j_common_ptr cinfo)
{
    std::array<char, JMSG_LENGTH_MAX> msg;
    (*cinfo->err->format_message)(cinfo, msg.data());
    std::array<wchar_t, JMSG_LENGTH_MAX> wmsg;
    ::MultiByteToWideChar(CP_UTF8, 0, msg.data(), -1, wmsg.data(), static_cast<int>(wmsg.size()));
    throw QuietException(wmsg.data());
}
void ClientConnection::DecompressJpegRect(int x, int y, const int w, const int h)
{
    struct jpeg_decompress_struct cinfo{};
    auto destroy = [](jpeg_decompress_struct* const p) {
        if (p->mem)
        {
            jpeg_destroy_decompress(p);
        }
    };
    std::unique_ptr<jpeg_decompress_struct, decltype(destroy)> cleanup(&cinfo, destroy);
    struct jpeg_error_mgr jerr;
    jpeg_source_mgr sourceManager{};
    bool jpegError{ false };
    JOCTET* jpegBufferPtr = nullptr;
    std::size_t jpegBufferLen{ 0U };
    cinfo.client_data = &jpegError;

    int compressedLen = static_cast<int>(ReadCompactLen());
    if (compressedLen <= 0)
    {
        vnclog.Print(0, _T("Incorrect data received from the server.\n"));
        return;
    }

    CheckBufferSize(static_cast<UINT>(compressedLen));
    ReadExact(m_netbuf, compressedLen);

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = JpegErrorHeader;
    try
    {
        jpeg_create_decompress(&cinfo);

        //JpegSetSrcManager(&cinfo, m_netbuf, compressedLen);

        jpegBufferPtr = reinterpret_cast<JOCTET*>(m_netbuf);
        jpegBufferLen = static_cast<std::size_t>(compressedLen);

        sourceManager.init_source = JpegInitSource;
        sourceManager.fill_input_buffer = JpegFillInputBuffer;
        sourceManager.skip_input_data = JpegSkipInputData;
        sourceManager.resync_to_restart = jpeg_resync_to_restart;
        sourceManager.term_source = JpegTermSource;
        sourceManager.next_input_byte = jpegBufferPtr;
        sourceManager.bytes_in_buffer = jpegBufferLen;

        cinfo.src = &sourceManager;

        jpeg_read_header(&cinfo, TRUE);
    }
    catch (QuietException& e)
    {
        e.Report();
        return;
    }

    cinfo.out_color_space = JCS_RGB;

    jpeg_start_decompress(&cinfo);
    if ((static_cast<int>(cinfo.output_width) != w) || (static_cast<int>(cinfo.output_height) != h) ||
        (cinfo.output_components != 3))
    {
        vnclog.Print(0, _T("Tight Encoding: Wrong JPEG data received.\n"));
        return;
    }

    if ((w < 1) || (h < 1) || (w > 65535))
    {
        vnclog.Print(0, _T("Tight JPEG: invalid rectangle dimensions %dx%d\n"), w, h);
        return;
    }

    omni_mutex_lock l(m_bitmapdcMutex);

    // Two scanlines: for 24bit and COLORREF samples
    // adzm 2010-08 - Increase zlib buffer size (from TightVNC)
    CheckZlibBufferSize(2 * w * 4);

    std::array<JSAMPROW, 1U> rowPointer;
    rowPointer[0U] = static_cast<JSAMPROW>(m_zlibbuf);

    COLORREF* pixelPtr;
    for (int dy{ 0 }; cinfo.output_scanline < cinfo.output_height; ++dy)
    {
        jpeg_read_scanlines(&cinfo, rowPointer.data(), 1);
        if (jpegError)
        {
            break;
        }
        pixelPtr = reinterpret_cast<COLORREF*>(&m_zlibbuf[w * 4]);
        for (int dx{ 0 }; dx < w; ++dx)
        {
            *pixelPtr++ = COLOR_FROM_PIXEL24_ADDRESS(&m_zlibbuf[dx * 3]);
        }
        SETPIXELS_NOCONV(&m_zlibbuf[w * 4], x, y + dy, w, 1)
    }

    if (!jpegError)
    {
        jpeg_finish_decompress(&cinfo);
    }
}
