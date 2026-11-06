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

#include "vncEncodeZlib.h"

#include "../../common/UltraVncZ.h"

#include <cstring>
//------------------------------------------------------------------

vncEncodeZlib::vncEncodeZlib()
{
    m_queueEnable = false;
    must_be_zipped = false;
    ultraVncZ = new UltraVncZ();
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
}
//------------------------------------------------------------------
vncEncodeZlib::~vncEncodeZlib()
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
    vnclog.Print(LL_INTINFO, VNCLOG("Zlib Xor encoder stats: rawdata=%d  protocol=%d compressed=%d transmitted=%d\n"), dataSize,
                 rectangleOverhead, encodedSize, transmittedSize);
    if (dataSize != 0)
    {
        vnclog.Print(LL_INTINFO, VNCLOG("Zlib Xor encoder efficiency: %.3f%%\n"),
                     static_cast<double>((static_cast<double>(((dataSize - transmittedSize) * 100)) / dataSize)));
    }
    delete ultraVncZ;
}
//------------------------------------------------------------------
void vncEncodeZlib::Init()
{
    totalraw = 0;
    encodedSize = 0;
    rectangleOverhead = 0;
    transmittedSize = 0;
    dataSize = 0;
    vncEncoder::Init();
    m_nNbRects = 0;
}
//------------------------------------------------------------------
UINT vncEncodeZlib::RequiredBuffSize(const UINT width, const UINT height)
{
    int result;

    // The zlib library specifies a maximum compressed size of
    // the raw size plus one percent plus 8 bytes. We also need
    // to cover the zlib header space.
    result = vncEncoder::RequiredBuffSize(width, height);
    Firstrun = result * 2; //Needed to exclude xor when cachebuffer is empty
    result += ((result / 100) + 8) + sz_rfbZlibHeader;
    return static_cast<UINT>(result);
}
//------------------------------------------------------------------
UINT vncEncodeZlib::NumCodedRects(const rfb::Rect& rect)
{
    const int rectW = rect.br.x - rect.tl.x;
    const int rectH = rect.br.y - rect.tl.y;
    int aantal = (static_cast<int>((rectH - 1) / (ultraVncZ->maxSize(rectW * rectH) / rectW) + 1));
    m_queueEnable = false;
    if (m_use_lastrect && (aantal > 1) && m_allow_queue)
    {
        m_queueEnable = true;
        return 0;
    }
    // Return the number of rectangles needed to encode the given
    // update. ( ZLIB_MAX_SIZE(rectW) / rectW ) is the number of lines in
    // each maximum size rectangle.
    // When solid is enabled, most of the pixels are removed
    return ((static_cast<UINT>(rectH - 1)) / (ultraVncZ->maxSize(static_cast<UINT>(rectW * rectH)) / static_cast<UINT>(rectW)) + 1);
}
//------------------------------------------------------------------
inline UINT vncEncodeZlib::EncodeRect(BYTE* const source, VSocket* const outConn, BYTE* const dest, const rfb::Rect& rect,
                                      bool allow_queue)
{
    m_allow_queue = allow_queue;
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
    maxLines = static_cast<int>(ultraVncZ->maxSize(rectW * rectH) / rectW);
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
        partialSize = static_cast<int>(EncodeOneRect(source, dest, partialRect, outConn));
        totalSize += partialSize;
        linesRemaining -= linesToComp;
        partialRect.top += linesToComp;
        if ((linesRemaining > 0) && (partialSize > 0))
        {
            outConn->SendExactQueue(reinterpret_cast<char*>(dest), static_cast<VCard>(partialSize));
            transmittedSize += partialSize;
        }
    }
    transmittedSize += partialSize;
    return static_cast<UINT>(partialSize);
}
//------------------------------------------------------------------
inline UINT vncEncodeZlib::EncodeOneRect(BYTE* const source, BYTE* const dest, const RECT& rect, VSocket* const outConn)
{
    int totalCompDataLen{ 0 };
    UINT avail_in{ 0U };
    const int rectW = rect.right - rect.left;
    const int rectH = rect.bottom - rect.top;
    const int rawDataSize = (rectW * rectH * m_remoteformat.bitsPerPixel / 8);
    const int maxCompSize = (rawDataSize + (rawDataSize / 100) + 8);
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
    surh->encoding = static_cast<unsigned int>(Swap32IfLE(m_use_zstd ? rfbEncodingZstd : rfbEncodingZlib));
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

    avail_in = static_cast<UINT>(rawDataSize);
    Translate(source, m_buffer, rect);
    if (static_cast<UINT>(rawDataSize) < ultraVncZ->minSize())
    {
        return vncEncoder::EncodeRect(source, dest, rect);
    }

    if ((rawDataSize < 1000) && m_queueEnable)
    {
        surh->encoding = Swap32IfLE(rfbEncodingRaw);
        std::memcpy(dest + sz_rfbFramebufferUpdateRectHeader, m_buffer, static_cast<size_t>(rawDataSize));
        AddToQueu(dest, sz_rfbFramebufferUpdateRectHeader + rawDataSize, outConn, 1);
        return 0;
    }
    surh->encoding = static_cast<unsigned int>(Swap32IfLE(m_use_zstd ? rfbEncodingZstd : rfbEncodingZlib));
    totalCompDataLen = ultraVncZ->compress(m_compresslevel, avail_in, static_cast<UINT>(maxCompSize), m_buffer,
                                           (dest + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader));
    if (totalCompDataLen == 0)
    {
        return vncEncoder::EncodeRect(source, dest, rect);
    }
    rfbZlibHeader* zlibh = reinterpret_cast<rfbZlibHeader*>((dest + sz_rfbFramebufferUpdateRectHeader));
    zlibh->nBytes = static_cast<unsigned int>(Swap32IfLE(totalCompDataLen));
    // Update statistics
    encodedSize += sz_rfbZlibHeader + totalCompDataLen;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
    return sz_rfbFramebufferUpdateRectHeader + static_cast<UINT>(sz_rfbZlibHeader + totalCompDataLen);
}
//------------------------------------------------------------------
void vncEncodeZlib::AddToQueu(BYTE* const source, const int sizerect, VSocket* const outConn, const int updatetype)
{
    if (m_Queuelen + sizerect > (MaxQueuebufflen))
    {
        SendZlibrects(outConn);
    }
    std::memcpy(m_Queuebuffer + m_Queuelen, source, static_cast<size_t>(sizerect));
    m_Queuelen += sizerect;
    ++m_nNbRects;
    if (updatetype == 1)
    {
        must_be_zipped = true;
    }
    if (m_nNbRects > 50)
    {
        SendZlibrects(outConn);
    }
}
//------------------------------------------------------------------
void vncEncodeZlib::SendZlibrects(VSocket* const outConn)
{
    int NRects = m_nNbRects;
    const int rawDataSize = (m_Queuelen);
    UINT maxCompSize = (m_Queuelen + static_cast<UINT>((m_Queuelen / 100) + 8));
    if (NRects == 0)
    {
        return;
    } // NO update
    if ((m_nNbRects < 3) && !must_be_zipped)
    {
        outConn->SendExactQueue(reinterpret_cast<char*>(m_Queuebuffer), static_cast<VCard>(m_Queuelen)); // 1 Small update
        encodedSize += m_Queuelen - sz_rfbFramebufferUpdateRectHeader;
        rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
        m_nNbRects = 0;
        m_Queuelen = 0;
        return;
    }
    encodedSize += m_Queuelen - sz_rfbFramebufferUpdateRectHeader;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
    m_nNbRects = 0;
    m_Queuelen = 0;
    must_be_zipped = false;
    maxCompSize = ultraVncZ->compress(m_compresslevel, static_cast<UINT>(rawDataSize), maxCompSize, m_Queuebuffer, m_QueueCompressedbuffer);
    if (maxCompSize == 0)
    {
        return;
    }
    const int rawDataSize1 = rawDataSize / 65535;
    const int rawDataSize2 = rawDataSize % 65535;
    rfbFramebufferUpdateRectHeader CacheRectsHeader;
    CacheRectsHeader.r.x = static_cast<CARD16>((NRects));
    CacheRectsHeader.r.y = static_cast<CARD16>((rawDataSize2));
    CacheRectsHeader.r.w = static_cast<CARD16>((rawDataSize1));
    CacheRectsHeader.r.x = Swap16IfLE(CacheRectsHeader.r.x);
    CacheRectsHeader.r.y = Swap16IfLE(CacheRectsHeader.r.y);
    CacheRectsHeader.r.w = Swap16IfLE(CacheRectsHeader.r.w);
    CacheRectsHeader.r.h = 0;
    CacheRectsHeader.encoding = Swap32IfLE(m_use_zstd ? rfbEncodingQueueZstd : rfbEncodingQueueZip);
    // Format the ZlibHeader
    rfbZlibHeader CacheZipHeader;
    CacheZipHeader.nBytes = Swap32IfLE(maxCompSize);
    vnclog.Print(LL_INTINFO, VNCLOG("********QUEUEQUEUE********** %d %d %d\r\n"), maxCompSize, rawDataSize, NRects);
    outConn->SendExactQueue(reinterpret_cast<char*>(&CacheRectsHeader), sizeof(CacheRectsHeader));
    outConn->SendExactQueue(reinterpret_cast<char*>(&CacheZipHeader), sizeof(CacheZipHeader));
    outConn->SendExactQueue(reinterpret_cast<char*>(m_QueueCompressedbuffer), maxCompSize);
    // Update statistics
    encodedSize += sz_rfbZlibHeader + maxCompSize;
    rectangleOverhead += sz_rfbFramebufferUpdateRectHeader;
    transmittedSize += maxCompSize + sz_rfbFramebufferUpdateRectHeader + sz_rfbZlibHeader;
}
//------------------------------------------------------------------
void vncEncodeZlib::LastRect(VSocket* const outConn) { SendZlibrects(outConn); }
//------------------------------------------------------------------
void vncEncodeZlib::set_use_zstd(const bool enabled)
{
    ultraVncZ->set_use_zstd(enabled);
    vncEncoder::set_use_zstd(enabled);
}
//------------------------------------------------------------------
