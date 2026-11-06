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

#include "vncEncodeUltra.h"

#include <cstring>

#define IN_LEN (128 * 1024)
#define OUT_LEN (IN_LEN + IN_LEN / 64 + 16 + 3)
#define HEAP_ALLOC(var, size) lzo_align_t __LZO_MMODEL var[((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t)]
static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

vncEncodeUltra::vncEncodeUltra()
{
    must_be_zipped = false;
    m_buffer = nullptr;
    m_Queuebuffer = nullptr;
    m_QueueCompressedbuffer = nullptr;
    m_bufflen = 0;
    m_Queuelen = 0;
    MaxQueuebufflen = 128 * 1024;

    m_Queuebuffer = new BYTE[static_cast<unsigned long long>(MaxQueuebufflen + 1)];
    if (m_Queuebuffer == nullptr)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("Memory error"));
    }
    m_QueueCompressedbuffer = new BYTE[MaxQueuebufflen + static_cast<unsigned long long>((MaxQueuebufflen / 100) + 8)];
    if (m_QueueCompressedbuffer == nullptr)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("Memory error"));
    }
    lzo = false;
}

vncEncodeUltra::~vncEncodeUltra()
{
    if (m_buffer != nullptr)
    {
        delete[] m_buffer;
        m_buffer = nullptr;
    }

    if (m_Queuebuffer != nullptr)
    {
        delete[] m_Queuebuffer;
        m_Queuebuffer = nullptr;
    }

    if (m_QueueCompressedbuffer != nullptr)
    {
        delete[] m_QueueCompressedbuffer;
        m_QueueCompressedbuffer = nullptr;
    }

    vnclog.Print(LL_INTINFO, VNCLOG("Ultra  encoder stats: rawdata=%d  protocol=%d compressed=%d transmitted=%d\n"), dataSize,
                 rectangleOverhead, encodedSize, transmittedSize);

    if (dataSize != 0)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("Ultra  encoder efficiency: %.3f%%\n"),
                     static_cast<double>((static_cast<double>(((dataSize - transmittedSize) * 100)) / dataSize)));
    }
}

void vncEncodeUltra::Init()
{
    totalraw = 0;
    encodedSize = 0;
    rectangleOverhead = 0;
    transmittedSize = 0;
    dataSize = 0;
    vncEncoder::Init();
    m_nNbRects = 0;
    m_queueEnable = true; // sf@2003
}

UINT vncEncodeUltra::RequiredBuffSize(const UINT width, const UINT height)
{
    int result;

    // The Ultra library specifies a maximum compressed size of
    // the raw size plus one percent plus 8 bytes. We also need
    // to cover the Ultra header space.
    result = vncEncoder::RequiredBuffSize(width, height);
    result += result / 64 + 16 + 3 + sz_rfbZlibHeader + sz_rfbFramebufferUpdateRectHeader;
    return static_cast<UINT>(result);
}

UINT vncEncodeUltra::NumCodedRects(const rfb::Rect& rect)
{
    return 0;
    const int rectW = rect.br.x - rect.tl.x;
    const int rectH = rect.br.y - rect.tl.y;
    return (static_cast<UINT>((rectH - 1) / (Ultra_MAX_SIZE(rectW) / rectW) + 1));
}

/*****************************************************************************
 *
 * Routines to implement Ultra Encoding (LZ+Huffman compression) by calling
 * the included Ultra library.
 */

// Encode the rectangle using Ultra compression
inline UINT vncEncodeUltra::EncodeRect(BYTE* const source, VSocket* const outConn, BYTE* const dest, const rfb::Rect& rect)
{

    int totalSize{ 0 };
    int partialSize{ 0 };
    int maxLines;
    int linesRemaining;
    RECT partialRect;
    const int rectW = rect.br.x - rect.tl.x;
    const int rectH = rect.br.y - rect.tl.y;

    partialRect.right = rect.br.x;
    partialRect.left = rect.tl.x;
    partialRect.top = rect.tl.y;
    partialRect.bottom = rect.br.y;

    /* WBB: For testing purposes only! */
    // vnclog.Print(LL_INTINFO, VNCLOG("rect.right=%d rect.left=%d rect.top=%d rect.bottom=%d\n"), rect.right, rect.left, rect.top,
    // rect.bottom);

    if (rectW == 0)
    {
        return 0;
    }
    if (rectH == 0)
    {
        return 0;
    }
    maxLines = (Ultra_MAX_SIZE(rectW) / rectW);
    linesRemaining = rectH;

    while (linesRemaining > 0)
    {

        int linesToComp;

        if (maxLines < linesRemaining)
        {
            linesToComp = maxLines;
        }
        else
        {
            linesToComp = linesRemaining;
        }

        partialRect.bottom = partialRect.top + linesToComp;

        /* WBB: For testing purposes only! */
        // vnclog.Print(LL_INTINFO, VNCLOG("partialRect.right=%d partialRect.left=%d partialRect.top=%d partialRect.bottom=%d\n"),
        // partialRect.right, partialRect.left, partialRect.top, partialRect.bottom);

        partialSize = static_cast<int>(EncodeOneRect(source, dest, partialRect, outConn));
        totalSize += partialSize;

        linesRemaining -= linesToComp;
        partialRect.top += linesToComp;

        if ((linesRemaining > 0) && (partialSize > 0))
        {
            // Send the encoded data
            outConn->SendExactQueue(reinterpret_cast<char*>(dest), static_cast<VCard>(partialSize));
            transmittedSize += partialSize;
        }
    }
    transmittedSize += partialSize;

    return static_cast<UINT>(partialSize);
}

// Encode the rectangle using Zlib compression
inline UINT vncEncodeUltra::EncodeOneRect(BYTE* const source, BYTE* const dest, const RECT& rect, VSocket* const outConn)
{
    const int rectW = rect.right - rect.left;
    const int rectH = rect.bottom - rect.top;
    const int rawDataSize = (rectW * rectH * m_remoteformat.bitsPerPixel / 8);

    if (!outConn->m_pIntegratedPluginInterface)
    {
        m_queueEnable = false;
    }
    // Create the rectangle header
    rfbFramebufferUpdateRectHeader* surh = reinterpret_cast<rfbFramebufferUpdateRectHeader*>(dest);
    // Modif rdv@2002 - v1.1.x - Application Resize
    surh->r.x = static_cast<CARD16>(static_cast<CARD16>(rect.left) - monitor_Offsetx);
    surh->r.y = static_cast<CARD16>(static_cast<CARD16>(rect.top) - monitor_Offsety);
    surh->r.w = static_cast<CARD16>((rectW));
    surh->r.h = static_cast<CARD16>((rectH));
    surh->r.x = Swap16IfLE(surh->r.x);
    surh->r.y = Swap16IfLE(surh->r.y);
    surh->r.w = Swap16IfLE(surh->r.w);
    surh->r.h = Swap16IfLE(surh->r.h);
    surh->encoding = Swap32IfLE(rfbEncodingUltra);

    dataSize += (rectW * rectH * m_remoteformat.bitsPerPixel) / 8;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;

    // create a space big enough for the Zlib encoded pixels
    if (m_bufflen < rawDataSize)
    {
        if (m_buffer != nullptr)
        {
            delete[] m_buffer;
            m_buffer = nullptr;
        }
        m_buffer = new BYTE[static_cast<unsigned long long>(rawDataSize + 1000)];
        if (m_buffer == nullptr)
        {
            return vncEncoder::EncodeRect(source, dest, rect);
        }
        m_bufflen = rawDataSize + 999;
    }
    // Translate the data into our new buffer
    Translate(source, m_buffer, rect);

    // Perhaps we can queue the small updates and compress them combined
    if (rawDataSize < VNC_ENCODE_ULTRA_MIN_COMP_SIZE)
    {
        if (m_queueEnable)
        {
            surh->encoding = Swap32IfLE(rfbEncodingRaw);
            std::memcpy(dest + sz_rfbFramebufferUpdateRectHeader, m_buffer, static_cast<size_t>(rawDataSize));
            AddToQueu(dest, sz_rfbFramebufferUpdateRectHeader + rawDataSize, outConn, 0);
            return 0;
        }
        else
        {
            return vncEncoder::EncodeRect(source, dest, rect);
        }
    }


    if ((rawDataSize < 1000) && m_queueEnable)
    {
        surh->encoding = Swap32IfLE(rfbEncodingRaw);
        std::memcpy(dest + sz_rfbFramebufferUpdateRectHeader, m_buffer, static_cast<size_t>(rawDataSize));
        AddToQueu(dest, sz_rfbFramebufferUpdateRectHeader + rawDataSize, outConn, 1);
        return 0;
    }

    surh->encoding = Swap32IfLE(rfbEncodingUltra);

    if (lzo == false)
    {
        if (lzo_init() == LZO_E_OK)
        {
            lzo = true;
        }
    }
    lzo1x_1_compress(m_buffer, static_cast<lzo_uint>(rawDataSize), dest + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader, &out_len, wrkmem);
    if (out_len > static_cast<lzo_uint>(rawDataSize))
    {
        return vncEncoder::EncodeRect(source, dest, rect);
    }

    // Format the ZlibHeader
    rfbZlibHeader* zlibh = reinterpret_cast<rfbZlibHeader*>((dest + sz_rfbFramebufferUpdateRectHeader));
    zlibh->nBytes = Swap32IfLE(out_len);

    // Update statistics
    encodedSize += sz_rfbZlibHeader + out_len;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;

    // Return the amount of data sent
    return static_cast<UINT>(sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader + out_len);
}



void vncEncodeUltra::AddToQueu(BYTE* const source, const int sizerect, VSocket* const outConn, const int updatetype)
{
    if (m_Queuelen + sizerect > (MaxQueuebufflen))
    {
        SendUltrarects(outConn);
    }
    std::memcpy(m_Queuebuffer + m_Queuelen, source, static_cast<size_t>(sizerect));
    m_Queuelen += sizerect;
    ++m_nNbRects;
    if (updatetype == 1)
    {
        must_be_zipped = true;
    }
    if (m_nNbRects > 0)
    {
        SendUltrarects(outConn);
    }
}

void vncEncodeUltra::AddToQueu2(BYTE* const source, const int sizerect, VSocket* const outConn, [[maybe_unused]] const int updatetype)
{
    BYTE* databegin = m_QueueCompressedbuffer + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader;
    rfbFramebufferUpdateRectHeader* CacheRectsHeader = reinterpret_cast<rfbFramebufferUpdateRectHeader*>(m_QueueCompressedbuffer);
    rfbZlibHeader* CacheZipHeader = reinterpret_cast<rfbZlibHeader*>(m_QueueCompressedbuffer) + sz_rfbFramebufferUpdateRectHeader;
    const lzo_uint rawDataSize = (static_cast<lzo_uint>(sizerect));
    lzo1x_1_compress(source, rawDataSize, databegin, &out_len, wrkmem);

    if (out_len > rawDataSize)
    {
        outConn->SendExactQueue(reinterpret_cast<char*>(source), static_cast<VCard>(sizerect)); // 1 Small update
        encodedSize += sizerect - sz_rfbFramebufferUpdateRectHeader;
        rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
        return;
    }

    const int rawDataSize1 = static_cast<int>(rawDataSize / 65535);
    const int rawDataSize2 = rawDataSize % 65535;

    CacheRectsHeader->r.x = static_cast<CARD16>((1));
    CacheRectsHeader->r.y = static_cast<CARD16>((rawDataSize2));
    CacheRectsHeader->r.w = static_cast<CARD16>((rawDataSize1));
    CacheRectsHeader->r.x = Swap16IfLE(CacheRectsHeader->r.x);
    CacheRectsHeader->r.y = Swap16IfLE(CacheRectsHeader->r.y);
    CacheRectsHeader->r.w = Swap16IfLE(CacheRectsHeader->r.w);
    CacheRectsHeader->r.h = 0;
    CacheRectsHeader->encoding = Swap32IfLE(rfbEncodingUltraZip);

    // Format the UltraHeader
    CacheZipHeader->nBytes = Swap32IfLE(out_len);

    vnclog.Print(LL_INTINFO, VNCLOG("********QUEUEQUEUE********** %d %d %d\r\n"), out_len, rawDataSize, 1);
    outConn->SendExactQueue(reinterpret_cast<char*>(m_QueueCompressedbuffer), static_cast<VCard>(out_len + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader));
    // Update statistics
    encodedSize += sz_rfbZlibHeader + out_len;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
    transmittedSize += out_len + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader;
}

void vncEncodeUltra::SendUltrarects(VSocket* const outConn)
{
    int NRects = m_nNbRects;
    const lzo_uint rawDataSize = (static_cast<lzo_uint>(m_Queuelen));

    if (NRects == 0)
    {
        return;
    } // NO update
    if ((m_nNbRects < 3) && !must_be_zipped)
    {
        encodedSize += m_Queuelen - sz_rfbFramebufferUpdateRectHeader;
        rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
        outConn->SendExactQueue(reinterpret_cast<char*>(m_Queuebuffer), static_cast<VCard>(m_Queuelen)); // 1 Small update
        m_nNbRects = 0;
        m_Queuelen = 0;
        return;
    }
    encodedSize += m_Queuelen - sz_rfbFramebufferUpdateRectHeader;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
    m_nNbRects = 0;
    m_Queuelen = 0;
    must_be_zipped = false;

    lzo1x_1_compress(m_Queuebuffer, rawDataSize, m_QueueCompressedbuffer, &out_len, wrkmem);

    if (out_len > rawDataSize)
    {
        outConn->SendExactQueue(reinterpret_cast<char*>(m_Queuebuffer), static_cast<VCard>(m_Queuelen)); // 1 Small update
        m_nNbRects = 0;
        m_Queuelen = 0;
        return;
    }

    const int rawDataSize1 = static_cast<int>(rawDataSize / 65535);
    const int rawDataSize2 = rawDataSize % 65535;

    rfbFramebufferUpdateRectHeader CacheRectsHeader;
    CacheRectsHeader.r.x = static_cast<CARD16>((NRects));
    CacheRectsHeader.r.y = static_cast<CARD16>((rawDataSize2));
    CacheRectsHeader.r.w = static_cast<CARD16>((rawDataSize1));
    CacheRectsHeader.r.x = Swap16IfLE(CacheRectsHeader.r.x);
    CacheRectsHeader.r.y = Swap16IfLE(CacheRectsHeader.r.y);
    CacheRectsHeader.r.w = Swap16IfLE(CacheRectsHeader.r.w);
    CacheRectsHeader.r.h = 0;
    CacheRectsHeader.encoding = Swap32IfLE(rfbEncodingUltraZip);

    // Format the UltraHeader
    rfbZlibHeader CacheZipHeader;
    CacheZipHeader.nBytes = Swap32IfLE(out_len);

    vnclog.Print(LL_INTINFO, VNCLOG("********QUEUEQUEUE********** %d %d %d\r\n"), out_len, rawDataSize, NRects);
    outConn->SendExactQueue(reinterpret_cast<char*>(&CacheRectsHeader), sizeof(CacheRectsHeader));
    outConn->SendExactQueue(reinterpret_cast<char*>(&CacheZipHeader), sizeof(CacheZipHeader));
    outConn->SendExactQueue(reinterpret_cast<char*>(m_QueueCompressedbuffer), static_cast<VCard>(out_len));
    // Update statistics
    encodedSize += sz_rfbZlibHeader + out_len;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
    transmittedSize += out_len + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader;
}

void vncEncodeUltra::LastRect(VSocket* const outConn) { SendUltrarects(outConn); }
