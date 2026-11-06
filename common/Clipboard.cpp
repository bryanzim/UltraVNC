// This file is part of UltraVNC
// https://github.com/ultravnc/UltraVNC
// https://uvnc.com/
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Copyright (C) 2002-2025 UltraVNC Team Members. All Rights Reserved.
// SPDX-FileCopyrightText: Copyright (C) 1999-2002 Vdacc-VNC & eSVNC Projects. All Rights Reserved.
//


// Clipboard.h

// adzm - July 2010
//
// Common classes for dealing with the clipboard, including serializing and deserializing compressed data, hashing and comparing,
// etc. Used by server and viewer.

#include "Clipboard.h"

#include <array>
#include <cstddef>
#include <cstring>

#define VC_EXTRALEAN
#include <rdr/MemInStream.h>
#include <rdr/ZlibInStream.h>
#include <rdr/ZlibOutStream.h>

#include <WinSock2.h>

#include <Windows.h>

#include <string>
#include <vector>

#ifdef _VCPKG
#include <zlib.h>
#include <zstd.h>

#else
#include "../zlib/zlib.h"
#include "../zstd/lib/zstd.h"

#endif





ExtendedClipboardDataMessage::ExtendedClipboardDataMessage()
    : m_pExtendedData{ nullptr }, m_nInternalLength{ 0 }, m_nReceivedLength{ 0 }, m_pCurrentPos{ nullptr }, m_pData{ nullptr }
{
    EnsureBufferLength(sz_rfbExtendedClipboardData);
    m_pCurrentPos += sz_rfbExtendedClipboardData;
}

ExtendedClipboardDataMessage::~ExtendedClipboardDataMessage() { delete[] m_pData; }

void ExtendedClipboardDataMessage::Reset()
{
    delete[] m_pData;
    m_pData = nullptr;
    m_pCurrentPos = nullptr;
    m_pExtendedData = nullptr;
    m_pCurrentPos = nullptr;
    m_nInternalLength = 0;
    m_nReceivedLength = 0;

    EnsureBufferLength(sz_rfbExtendedClipboardData);
    m_pCurrentPos += sz_rfbExtendedClipboardData;
}

void ExtendedClipboardDataMessage::AddFlag(const CARD32 flag) { m_pExtendedData->flags |= Swap32IfLE(flag); }

bool ExtendedClipboardDataMessage::HasFlag(const CARD32 flag) { return (m_pExtendedData->flags & Swap32IfLE(flag)) ? true : false; }

int ExtendedClipboardDataMessage::GetMessageLength() { return GetDataLength() - sz_rfbExtendedClipboardData; }

int ExtendedClipboardDataMessage::GetDataLength() { return m_pData ? static_cast<int>((m_pCurrentPos - m_pData)) : 0; }

const BYTE* ExtendedClipboardDataMessage::GetData() { return m_pData; }

BYTE* ExtendedClipboardDataMessage::GetBuffer() { return m_pData; }

int ExtendedClipboardDataMessage::GetBufferLength() { return m_nInternalLength; }

const BYTE* ExtendedClipboardDataMessage::GetCurrentPos() { return m_pCurrentPos; }

void ExtendedClipboardDataMessage::AppendInt(CARD32 val)
{
    EnsureBufferLength(static_cast<int>(GetDataLength() + sizeof(val)));

    val = Swap32IfLE(val);

    std::memcpy(m_pCurrentPos, &val, sizeof(val));
    m_pCurrentPos += sizeof(val);
}

void ExtendedClipboardDataMessage::AppendBytes(BYTE* const pData, const int length)
{
    EnsureBufferLength(GetDataLength() + length, false);

    std::memcpy(m_pCurrentPos, pData, static_cast<size_t>(length));
    m_pCurrentPos += length;
}

void ExtendedClipboardDataMessage::Advance(const int len)
{
    EnsureBufferLength(GetDataLength() + len, false);
    m_pCurrentPos += len;
}

CARD32 ExtendedClipboardDataMessage::ReadInt()
{
    CARD32 val = 0;

    std::memcpy(&val, m_pCurrentPos, sizeof(val));

    m_pCurrentPos += sizeof(val);

    return Swap32IfLE(val);
}

void ExtendedClipboardDataMessage::EnsureBufferLength(const int len, const bool bGrowBeyond)
{
    if (m_nInternalLength < len)
    {
        int nCurrentOffset = GetDataLength();

        int nNewLength = bGrowBeyond ? len * 2 : len;
        BYTE* pNewBuffer = new BYTE[static_cast<unsigned long long>(nNewLength)];
        ::ZeroMemory(pNewBuffer, static_cast<size_t>(nNewLength));

        if (m_pData)
        {
            std::memcpy(pNewBuffer, m_pData, static_cast<size_t>(GetDataLength()));
            delete[] m_pData;
        }

        m_pData = pNewBuffer;
        m_pExtendedData = reinterpret_cast<rfbExtendedClipboardData*>(m_pData);
        m_pCurrentPos = m_pData + nCurrentOffset;
        m_nInternalLength = nNewLength;
    }
}

bool ExtendedClipboardDataMessage::SetReceivedLength(const int len)
{
    if ((len < sz_rfbExtendedClipboardData) || (len > maxPayload) || (len > m_nInternalLength))
    {
        return false;
    }
    m_nReceivedLength = len;
    m_pCurrentPos = m_pData + sz_rfbExtendedClipboardData;
    return IsValid();
}

int ExtendedClipboardDataMessage::GetRemainingLength() { return m_nReceivedLength - GetDataLength(); }

bool ExtendedClipboardDataMessage::IsValid()
{
    if (m_nReceivedLength < sz_rfbExtendedClipboardData)
    {
        return false;
    }
    CARD32 action = GetFlags() & static_cast<CARD32>(clipActionMask);
    if (action & clipCaps)
    {
        return m_nReceivedLength == sz_rfbExtendedClipboardData + 4 * CountFormats();
    }
    if (!action || (action & (action - 1)))
    {
        return false;
    }
    return action == clipProvide ? m_nReceivedLength > sz_rfbExtendedClipboardData
                                 : m_nReceivedLength == sz_rfbExtendedClipboardData;
}

int ExtendedClipboardDataMessage::CountFormats()
{
    CARD32 flags = GetFlags();

    flags = flags & 0x0000FFFF;

    // from Brian W. Kernighan
    int c{ 0 };
    for (c = 0; flags; ++c)
    {
        flags &= flags - 1;
    }

    return c;
}

CARD32 ExtendedClipboardDataMessage::GetFlags() { return Swap32IfLE(m_pExtendedData->flags); }

const UINT ClipboardSettings::formatUnicodeText = CF_UNICODETEXT;
const UINT ClipboardSettings::formatRTF = ::RegisterClipboardFormatW(L"Rich Text Format");
const UINT ClipboardSettings::formatHTML = ::RegisterClipboardFormatW(L"HTML Format");
const UINT ClipboardSettings::formatDIB = CF_DIBV5;

const int ClipboardSettings::defaultLimitText =
    (static_cast<int>(0x00A00000)); // 10 megabytes uncompressed. Pretty huge, but not a problem for
                                    // a LAN. Better than the previous no limit, though.
const int ClipboardSettings::defaultLimitRTF = (static_cast<int>(
    0x00200000)); // Limit these to 2 megabytes so they don't end up adding too much strain by being enabled by default
const int ClipboardSettings::defaultLimitHTML =
    (static_cast<int>(0x00200000)); // besides, every 2mb rtf/html data I have seen will DEFLATE very well.
const int ClipboardSettings::defaultLimitDIB = (static_cast<int>(0x00000000)); // no DIB by default

const int ClipboardSettings::defaultLimit = (static_cast<int>(0x00200000));

ClipboardSettings::ClipboardSettings(CARD32 caps)
    : m_bSupportsEx{ false }, m_nLimitText(defaultLimitText), m_nLimitRTF(defaultLimitRTF), m_nLimitHTML(defaultLimitHTML),
      m_nLimitDIB(defaultLimitDIB), m_nRequestedLimitText(20 * 1024 * 1024), m_nRequestedLimitRTF{ 0 }, m_nRequestedLimitHTML{ 0 },
      m_nRequestedLimitDIB{ 0 }, m_myCaps(caps),
      m_remoteCaps(clipText | clipRTF | clipHTML | clipRequest | clipNotify | clipProvide)
{
}

CARD32 ClipboardSettings::defaultCaps = (clipCaps | clipRequest | clipProvide) // capabilities
                                        |
                                        (clipText | clipRTF | clipHTML | clipDIB); // supports Unicode text, RTF, and HTML, and DIB
CARD32 ClipboardSettings::defaultViewerCaps = defaultCaps | clipNotify | clipPeek;
CARD32 ClipboardSettings::defaultServerCaps = defaultCaps | clipPeek | clipNotify;


void ClipboardSettings::PrepareCapsPacket(ExtendedClipboardDataMessage& extendedDataMessage)
{
    // messages and formats that we can handle
    extendedDataMessage.m_pExtendedData->flags = Swap32IfLE(m_myCaps);

    // now include our limits in order of enum value
    const int limits[] = { m_nLimitText, m_nLimitRTF, m_nLimitHTML, m_nLimitDIB };
    for (int i{ 0 }; i < 16; ++i)
    {
        if (m_myCaps & (1u << i))
        {
            extendedDataMessage.AppendInt(((i < 4) && (limits[i] > 0) ? static_cast<CARD32>(limits[i]) : 0));
        }
    }
}

bool ClipboardSettings::HandleCapsPacket(ExtendedClipboardDataMessage& extendedDataMessage)
{
    if (!extendedDataMessage.IsValid() || !extendedDataMessage.HasFlag(clipCaps) ||
        (extendedDataMessage.GetRemainingLength() != 4 * extendedDataMessage.CountFormats()))
    {
        return false;
    }
    int nCount = extendedDataMessage.CountFormats();

    m_remoteCaps = extendedDataMessage.GetFlags();

    if (m_remoteCaps & clipText)
    {
        m_nRequestedLimitText = extendedDataMessage.ReadInt();
        --nCount;
    }
    else
    {
        m_nRequestedLimitText = 0;
    }
    if (m_remoteCaps & clipRTF)
    {
        m_nRequestedLimitRTF = extendedDataMessage.ReadInt();
        --nCount;
    }
    else
    {
        m_nRequestedLimitRTF = 0;
    }
    if (m_remoteCaps & clipHTML)
    {
        m_nRequestedLimitHTML = extendedDataMessage.ReadInt();
        --nCount;
    }
    else
    {
        m_nRequestedLimitHTML = 0;
    }
    if (m_remoteCaps & clipDIB)
    {
        m_nRequestedLimitDIB = extendedDataMessage.ReadInt();
        --nCount;
    }
    else
    {
        m_nRequestedLimitDIB = 0;
    }

    // read any unsupported values
    while (nCount)
    {
        extendedDataMessage.ReadInt();
        --nCount;
    }
    return true;
}

CARD32 ClipboardSettings::AutomaticFormats() const
{
    return (m_nLimitText > 0 ? clipText : 0) | (m_nLimitRTF > 0 ? clipRTF : 0) | (m_nLimitHTML > 0 ? clipHTML : 0) |
           (m_nLimitDIB > 0 ? clipDIB : 0);
}

ClipboardHolder::ClipboardHolder(const HWND hwndOwner) { m_bIsOpen = ::OpenClipboard(hwndOwner) ? true : false; }

ClipboardHolder::~ClipboardHolder()
{
    if (m_bIsOpen)
    {
        ::CloseClipboard();
    }
}

ClipboardData::ClipboardData()
    : m_crc{ 0 }, m_lengthText{ 0 }, m_lengthRTF{ 0 }, m_lengthHTML{ 0 }, m_lengthDIB{ 0 }, m_pDataText{ nullptr },
      m_pDataRTF{ nullptr }, m_pDataHTML{ nullptr }, m_pDataDIB{ nullptr }
{
}

ClipboardData::~ClipboardData() { FreeData(); }

void ClipboardData::FreeData()
{
    delete[] m_pDataText;
    delete[] m_pDataRTF;
    delete[] m_pDataHTML;
    delete[] m_pDataDIB;

    m_pDataText = nullptr;
    m_pDataRTF = nullptr;
    m_pDataHTML = nullptr;
    m_pDataDIB = nullptr;
}

bool ClipboardData::Load(const HWND hwndOwner, const bool allowEmpty) // will return false on failure
{
    // This can be improved by not making copies of the clipboard data and simply keeping the handles open; however
    // I seemed to notice some issues with having multiple clipboard format data handles open when an application
    // uses clipboard formats that it must render on demand. Regardless, I am not too concerned about this now.

    ClipboardHolder holder(hwndOwner);

    if (!holder.m_bIsOpen)
    {
        return false;
    }

    m_crc = 0;
    FreeData();

    m_crc = crc32(0L, Z_NULL, 0);

    // text
    {
        m_lengthText = 0;

        HANDLE hText = nullptr;
        if (::IsClipboardFormatAvailable(ClipboardSettings::formatUnicodeText))
        {
            hText = ::GetClipboardData(ClipboardSettings::formatUnicodeText);
        }

        if (allowEmpty && IsClipboardFormatAvailable(ClipboardSettings::formatUnicodeText) && !hText)
        {
            return false;
        }
        if (hText)
        {
            SIZE_T size = ::GlobalSize(hText);
            if (allowEmpty && (!size || (size > ExtendedClipboardDataMessage::maxPayload)))
            {
                return false;
            }
            BYTE* pData = reinterpret_cast<BYTE*>(::GlobalLock(hText));
            if (allowEmpty && !pData)
            {
                return false;
            }
            int nLength = static_cast<int>(size);

            if ((pData != nullptr) && (nLength > 0))
            {

                std::wstring normalized;
                LPCWSTR text = reinterpret_cast<LPCWSTR>(pData);
                if (allowEmpty)
                {
                    std::size_t count = wcsnlen_s(text, static_cast<unsigned long long>(nLength) / sizeof(wchar_t));
                    if (count == static_cast<unsigned long long>(nLength) / sizeof(wchar_t))
                    {
                        ::GlobalUnlock(hText);
                        return false;
                    }
                    for (std::size_t i{ 0U }; i < count; ++i)
                    {
                        if ((text[i] == L'\n') && ((i == 0) || (text[i - 1] != L'\r')))
                        {
                            normalized += L'\r';
                        }
                        normalized += text[i];
                        if ((text[i] == L'\r') && ((i + 1 == count) || (text[i + 1] != L'\n')))
                        {
                            normalized += L'\n';
                        }
                    }
                    text = normalized.c_str();
                }
                // Convert from UTF-16 to UTF-8
                int nConvertedSize = ::WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                if (allowEmpty && ((nConvertedSize <= 0) || (nConvertedSize > ExtendedClipboardDataMessage::maxPayload)))
                {
                    ::GlobalUnlock(hText);
                    return false;
                }

                if (nConvertedSize > 0)
                {
                    m_pDataText = new BYTE[static_cast<unsigned long long>(nConvertedSize)];
                    //memcpy(m_pDataText, pData, nLength);

                    int nFinalConvertedSize =
                        ::WideCharToMultiByte(CP_UTF8, 0, text, -1, reinterpret_cast<LPSTR>(m_pDataText), nConvertedSize, nullptr, nullptr);

                    if (nFinalConvertedSize > 0)
                    {
                        m_lengthText = nFinalConvertedSize;

                        m_crc = crc32(m_crc, m_pDataText, static_cast<uInt>(nFinalConvertedSize));
                    }
                    else
                    {
                        delete[] m_pDataText;
                        m_pDataText = nullptr;
                    }
                }

                ::GlobalUnlock(hText);
            }
        }
    }

    // RTF
    {
        m_lengthRTF = 0;

        HANDLE hRTF = nullptr;
        if (::IsClipboardFormatAvailable(ClipboardSettings::formatRTF))
        {
            hRTF = ::GetClipboardData(ClipboardSettings::formatRTF);
        }

        if (allowEmpty && IsClipboardFormatAvailable(ClipboardSettings::formatRTF) && !hRTF)
        {
            return false;
        }
        if (hRTF)
        {
            SIZE_T size = ::GlobalSize(hRTF);
            if (allowEmpty && (!size || (size > ExtendedClipboardDataMessage::maxPayload)))
            {
                return false;
            }
            BYTE* pData = reinterpret_cast<BYTE*>(::GlobalLock(hRTF));
            if (allowEmpty && !pData)
            {
                return false;
            }
            int nLength = static_cast<int>(size);

            if ((pData != nullptr) && (nLength > 0))
            {

                m_pDataRTF = new BYTE[static_cast<unsigned long long>(nLength)];
                std::memcpy(m_pDataRTF, pData, static_cast<size_t>(nLength));

                m_lengthRTF = nLength;

                ::GlobalUnlock(hRTF);
            }
        }
    }

    // HTML
    {
        m_lengthHTML = 0;

        HANDLE hHTML = nullptr;
        if (::IsClipboardFormatAvailable(ClipboardSettings::formatHTML))
        {
            hHTML = ::GetClipboardData(ClipboardSettings::formatHTML);
        }

        if (allowEmpty && IsClipboardFormatAvailable(ClipboardSettings::formatHTML) && !hHTML)
        {
            return false;
        }
        if (hHTML)
        {
            SIZE_T size = ::GlobalSize(hHTML);
            if (allowEmpty && (!size || (size > ExtendedClipboardDataMessage::maxPayload)))
            {
                return false;
            }
            BYTE* pData = reinterpret_cast<BYTE*>(::GlobalLock(hHTML));
            if (allowEmpty && !pData)
            {
                return false;
            }
            int nLength = static_cast<int>(size);

            if ((pData != nullptr) && (nLength > 0))
            {

                m_pDataHTML = new BYTE[static_cast<unsigned long long>(nLength)];
                std::memcpy(m_pDataHTML, pData, static_cast<size_t>(nLength));

                m_lengthHTML = nLength;

                ::GlobalUnlock(hHTML);
            }
        }
    }

    // DIB
    {
        m_lengthDIB = 0;

        HANDLE hDIB = nullptr;
        if (::IsClipboardFormatAvailable(ClipboardSettings::formatDIB))
        {
            hDIB = ::GetClipboardData(ClipboardSettings::formatDIB);
        }

        if (allowEmpty && IsClipboardFormatAvailable(ClipboardSettings::formatDIB) && !hDIB)
        {
            return false;
        }
        if (hDIB)
        {
            SIZE_T size = ::GlobalSize(hDIB);
            if (allowEmpty && (!size || (size > ExtendedClipboardDataMessage::maxPayload)))
            {
                return false;
            }
            BYTE* pData = reinterpret_cast<BYTE*>(::GlobalLock(hDIB));
            if (allowEmpty && !pData)
            {
                return false;
            }
            int nLength = static_cast<int>(size);

            if ((pData != nullptr) && (nLength > 0))
            {

                m_pDataDIB = new BYTE[static_cast<unsigned long long>(nLength)];
                std::memcpy(m_pDataDIB, pData, static_cast<size_t>(nLength));

                m_lengthDIB = nLength;

                ::GlobalUnlock(hDIB);
            }
        }
    }

    return allowEmpty || (m_lengthText + m_lengthRTF + m_lengthHTML + m_lengthDIB > 0);
}

CARD32 ClipboardData::Formats() const
{
    return (m_lengthText ? clipText : 0) | (m_lengthRTF ? clipRTF : 0) | (m_lengthHTML ? clipHTML : 0) |
           (m_lengthDIB ? clipDIB : 0);
}

DWORD ClipboardData::ExtendedCRC() const
{
    const int lengths[] = { m_lengthText, m_lengthRTF, m_lengthHTML, m_lengthDIB };
    const BYTE* buffers[] = { m_pDataText, m_pDataRTF, m_pDataHTML, m_pDataDIB };
    DWORD crc = crc32(0L, Z_NULL, 0);
    for (int i{ 0 }; i < 4; ++i)
    {
        crc = crc32(crc, reinterpret_cast<const BYTE*>(&lengths[i]), sizeof(lengths[i]));
        if (lengths[i] > 0)
        {
            crc = crc32(crc, buffers[i], static_cast<uInt>(lengths[i]));
        }
    }
    return crc;
}

bool ClipboardData::Decode(ExtendedClipboardDataMessage& message)
{
    if (!message.IsValid() || !message.HasFlag(clipProvide) || (message.GetRemainingLength() <= 0))
    {
        return false;
    }
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK)
    {
        return false;
    }
    stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef *>(message.GetCurrentPos()));
    stream.avail_in = static_cast<uInt>(message.GetRemainingLength());
    std::vector<BYTE> decoded;
    int result = Z_OK;
    do
    {
        std::array<BYTE, 16384U> chunk;
        stream.next_out = chunk.data();
        stream.avail_out = sizeof(chunk);
        uInt before = stream.avail_in;
        result = inflate(&stream, Z_SYNC_FLUSH);
        std::size_t count = sizeof(chunk) - stream.avail_out;
        if (((result != Z_OK) && (result != Z_STREAM_END) && (result != Z_BUF_ERROR)) ||
            (decoded.size() + count > ExtendedClipboardDataMessage::maxPayload) ||
            ((result == Z_STREAM_END) && (stream.avail_in != 0)))
        {
            inflateEnd(&stream);
            return false;
        }
        decoded.insert(decoded.end(), chunk.data(), chunk.data() + count);
        if ((result == Z_STREAM_END) || (!stream.avail_in && stream.avail_out))
        {
            break;
        }
        if (!count && (before == stream.avail_in))
        {
            inflateEnd(&stream);
            return false;
        }
    } while (true);
    inflateEnd(&stream);
    if (result != Z_STREAM_END)
    {
        int size = message.GetRemainingLength();
        const BYTE* bytes = message.GetCurrentPos();
        if ((size < 4) || bytes[size - 4] || bytes[size - 3] || (bytes[size - 2] != 0xff) || (bytes[size - 1] != 0xff))
        {
            return false;
        }
    }
    FreeData();
    m_lengthText = m_lengthRTF = m_lengthHTML = m_lengthDIB = 0;
    int* lengths[] = { &m_lengthText, &m_lengthRTF, &m_lengthHTML, &m_lengthDIB };
    BYTE** buffers[] = { &m_pDataText, &m_pDataRTF, &m_pDataHTML, &m_pDataDIB };
    std::size_t offset{ 0U };
    for (int i{ 0 }; i < 16; ++i)
    {
        if (!(message.GetFlags() & (1u << i)))
        {
            continue;
        }
        if (decoded.size() - offset < 4)
        {
            return false;
        }
        CARD32 size = (static_cast<CARD32>(decoded[offset]) << 24) | (static_cast<CARD32>(decoded[offset + 1]) << 16) | (static_cast<CARD32>(decoded[offset + 2]) << 8) |
                      decoded[offset + 3];
        offset += 4;
        if (size > decoded.size() - offset)
        {
            return false;
        }
        if ((i == 0) && (!size || (decoded[offset + size - 1] != 0) || std::memchr(decoded.data() + offset, 0, size - 1) ||
                         !::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<LPCSTR>(decoded.data()) + offset, static_cast<int>(size), nullptr, 0)))
        {
            return false;
        }
        if ((i < 4) && size)
        {
            *lengths[i] = static_cast<int>(size);
            *buffers[i] = new BYTE[size];
            std::memcpy(*buffers[i], decoded.data() + offset, size);
        }
        offset += size;
    }
    m_crc = crc32(0L, Z_NULL, 0);
    if (m_lengthText)
    {
        m_crc = crc32(m_crc, m_pDataText, static_cast<uInt>(m_lengthText));
    }
    return offset == decoded.size();
}

bool ClipboardData::Restore(const HWND hwndOwner, ExtendedClipboardDataMessage& extendedClipboardDataMessage)
{
    if (extendedClipboardDataMessage.GetRemainingLength() <= 0)
    {
        // no data beyond the flags
        return false;
    }
    if (!Decode(extendedClipboardDataMessage))
    {
        return false;
    }
    const int lengths[] = { m_lengthText, m_lengthRTF, m_lengthHTML, m_lengthDIB };
    const BYTE* buffers[] = { m_pDataText, m_pDataRTF, m_pDataHTML, m_pDataDIB };
    const UINT formats[] = { ClipboardSettings::formatUnicodeText, ClipboardSettings::formatRTF, ClipboardSettings::formatHTML,
                             ClipboardSettings::formatDIB };
    std::array<HGLOBAL, 4U> handles{};
    bool success{ true };
    for (int i{ 0 }; (i < 4) && success; ++i)
    {
        if (!lengths[i])
        {
            continue;
        }
        int count = lengths[i];
        if (i == 0)
        {
            // get the incoming UTF-8 text
            // now we have to translate to UTF-16
            count = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<LPCSTR>(buffers[i]), lengths[i], nullptr, 0);
        }
        handles[static_cast<unsigned long long>(i)] = ::GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE | GMEM_ZEROINIT, i == 0 ? static_cast<unsigned long long>(count) * sizeof(wchar_t) : static_cast<unsigned long long>(count));
        BYTE* target = handles[static_cast<unsigned long long>(i)] ? reinterpret_cast<BYTE*>(::GlobalLock(handles[i])) : nullptr;
        if (!target)
        {
            success = false;
            break;
        }
        if (i == 0)
        {
            success =
                ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, reinterpret_cast<LPCSTR>(buffers[i]), lengths[i], reinterpret_cast<LPWSTR>(target), count) == count;
        }
        else
        {
            std::memcpy(target, buffers[i], static_cast<size_t>(lengths[i]));
        }
        ::GlobalUnlock(handles[static_cast<unsigned long long>(i)]);
    }
    // we can ignore everything else
    if (success)
    {
        ClipboardHolder holder(hwndOwner);
        success = holder.m_bIsOpen && EmptyClipboard();
        for (int i{ 0 }; (i < 4) && success; ++i)
        {
            if (handles[static_cast<unsigned long long>(i)])
            {
                success = ::SetClipboardData(formats[i], handles[static_cast<unsigned long long>(i)]) != nullptr;
                if (success)
                {
                    handles[static_cast<unsigned long long>(i)] = nullptr;
                }
            }
        }
    }
    for (int i{ 0 }; i < 4; ++i)
    {
        if (handles[static_cast<unsigned long long>(i)])
        {
            ::GlobalFree(handles[static_cast<unsigned long long>(i)]);
        }
    }
    return success;
}

Clipboard::Clipboard(CARD32 caps)
    : m_requestedRemoteFormats{ 0 }, m_extendedCRC{ 0 }, m_hasExtendedCRC{ false }, settings(caps), m_crc{ 0 },
      m_bNeedToProvide{ false }, m_bNeedToNotify{ false }, m_notifiedRemoteFormats{ 0 }
{
}

void Clipboard::AcceptRemoteData(const ClipboardData& data)
{
    m_crc = data.m_crc;
    m_extendedCRC = data.ExtendedCRC();
    m_hasExtendedCRC = true;
    ResetTransferState();
}

void Clipboard::ResetTransferState()
{
    m_requestedRemoteFormats = 0;
    m_notifiedRemoteFormats = 0;
    m_bNeedToProvide = false;
    m_bNeedToNotify = false;
    extendedClipboardDataMessage.Reset();
    extendedClipboardDataNotifyMessage.Reset();
}

CARD32 Clipboard::RequestFormats(CARD32 formats, const bool automatic)
{
    if (!(settings.m_remoteCaps & clipRequest))
    {
        return 0;
    }
    formats &= settings.m_myCaps & (clipText | clipRTF | clipHTML | clipDIB);
    if (automatic)
    {
        formats &= settings.AutomaticFormats();
    }
    formats &= ~m_requestedRemoteFormats;
    m_requestedRemoteFormats |= formats;
    return formats;
}

bool Clipboard::UpdateExtended(ClipboardData& data, const CARD32 overrideFlags)
{
    DWORD crc = data.ExtendedCRC();
    if (!overrideFlags && m_hasExtendedCRC && (m_extendedCRC == crc))
    {
        return false;
    }
    CARD32 available = data.Formats();
    if (!overrideFlags)
    {
        available &= settings.AutomaticFormats();
    }
    if (overrideFlags & clipPeek)
    {
        extendedClipboardDataNotifyMessage.Reset();
        extendedClipboardDataNotifyMessage.AddFlag(clipNotify | available);
        m_bNeedToNotify = (settings.m_remoteCaps & clipNotify) != 0;
        return m_bNeedToNotify;
    }
    m_bNeedToProvide = false;
    m_bNeedToNotify = false;
    extendedClipboardDataMessage.Reset();
    extendedClipboardDataNotifyMessage.Reset();
    extendedClipboardDataMessage.AddFlag(clipProvide);
    extendedClipboardDataNotifyMessage.AddFlag(clipNotify | available);
    const int lengths[] = { data.m_lengthText, data.m_lengthRTF, data.m_lengthHTML, data.m_lengthDIB };
    BYTE* buffers[] = { data.m_pDataText, data.m_pDataRTF, data.m_pDataHTML, data.m_pDataDIB };
    const CARD32 limits[] = { settings.m_nRequestedLimitText, settings.m_nRequestedLimitRTF, settings.m_nRequestedLimitHTML,
                              settings.m_nRequestedLimitDIB };
    bool requested = (overrideFlags & clipRequest) != 0;
    CARD32 selected = available & (requested ? overrideFlags : settings.m_remoteCaps);
    if (!requested && (overrideFlags & clipFormatMask))
    {
        selected &= overrideFlags;
    }
    if (!requested && !overrideFlags)
    {
        selected &= settings.AutomaticFormats();
    }
    if (!(settings.m_remoteCaps & clipProvide))
    {
        selected = 0;
    }
    rdr::MemOutStream plain;
    CARD32 provided = 0;
    for (int i{ 0 }; i < 4; ++i)
    {
        CARD32 format = 1u << i;
        if (!(selected & format) || (!requested && (static_cast<CARD32>(lengths[i]) > limits[i])))
        {
            continue;
        }
        if (lengths[i] > ExtendedClipboardDataMessage::maxPayload - plain.length() - 4)
        {
            continue;
        }
        plain.writeU32(static_cast<rdr::U32>(lengths[i]));
        plain.writeBytes(buffers[i], lengths[i]);
        provided |= format;
    }
    if (provided || (requested && (settings.m_remoteCaps & clipProvide)))
    {
        uLongf size = compressBound(static_cast<uLong>(plain.length()));
        std::vector<BYTE> compressed(size);
        if ((compress2(compressed.data(), &size, reinterpret_cast<const Bytef*>(plain.data()), static_cast<uLong>(plain.length()), Z_BEST_COMPRESSION) == Z_OK) &&
            (size <= ExtendedClipboardDataMessage::maxPayload - 4))
        {
            extendedClipboardDataMessage.AddFlag(provided);
            extendedClipboardDataMessage.AppendBytes(compressed.data(), static_cast<int>(size));
            m_bNeedToProvide = true;
        }
    }
    if (!m_bNeedToProvide)
    {
        provided = 0;
    }
    m_bNeedToNotify = !requested && (settings.m_remoteCaps & clipNotify) && (!available || (available & ~provided));
    if (!requested)
    {
        m_extendedCRC = crc;
        m_hasExtendedCRC = true;
    }
    return m_bNeedToProvide || m_bNeedToNotify;
}

// returns true if something changed
bool Clipboard::UpdateClipTextEx(ClipboardData& clipboardData, CARD32 overrideFlags)
{
    if (settings.m_bSupportsEx)
    {
        return UpdateExtended(clipboardData, overrideFlags);
    }
    if ((m_crc == clipboardData.m_crc) && (overrideFlags == 0))
    {
        return false;
    }

    if (overrideFlags & clipPeek)
    {
        // don't reset anything regarding providing new data, just 'include' any
        // formats into the notify response.
        if (clipboardData.m_lengthText != 0)
        {
            extendedClipboardDataNotifyMessage.AddFlag(clipText);
            m_bNeedToNotify = true;
        }
        if (clipboardData.m_lengthRTF != 0)
        {
            extendedClipboardDataNotifyMessage.AddFlag(clipRTF);
            m_bNeedToNotify = true;
        }
        if (clipboardData.m_lengthHTML != 0)
        {
            extendedClipboardDataNotifyMessage.AddFlag(clipHTML);
            m_bNeedToNotify = true;
        }
        if (clipboardData.m_lengthDIB != 0)
        {
            extendedClipboardDataNotifyMessage.AddFlag(clipDIB);
            m_bNeedToNotify = true;
        }
    }
    else
    {

        m_bNeedToProvide = false;
        m_bNeedToNotify = false;
        extendedClipboardDataMessage.Reset();
        extendedClipboardDataNotifyMessage.Reset();

        extendedClipboardDataMessage.AddFlag(clipProvide);
        extendedClipboardDataNotifyMessage.AddFlag(clipNotify);

        rdr::MemOutStream memStream;
        {
            rdr::ZlibOutStream compressedStream(&memStream, 0, 9); //Z_BEST_COMPRESSION

            if (clipboardData.m_lengthText != 0)
            {
                extendedClipboardDataNotifyMessage.AddFlag(clipText);

                if ((clipboardData.m_lengthText <= settings.m_nLimitText) || (overrideFlags & clipText))
                {
                    compressedStream.writeU32(static_cast<rdr::U32>(clipboardData.m_lengthText));
                    compressedStream.writeBytes(clipboardData.m_pDataText, clipboardData.m_lengthText);
                    extendedClipboardDataMessage.AddFlag(clipText);
                    m_bNeedToProvide = true;
                }
                else if (!(overrideFlags & clipRequest))
                {
                    m_bNeedToNotify = true;
                }
            }
            if (clipboardData.m_lengthRTF != 0)
            {
                extendedClipboardDataNotifyMessage.AddFlag(clipRTF);

                if ((clipboardData.m_lengthRTF <= settings.m_nLimitRTF) || (overrideFlags & clipRTF))
                {
                    compressedStream.writeU32(static_cast<rdr::U32>(clipboardData.m_lengthRTF));
                    compressedStream.writeBytes(clipboardData.m_pDataRTF, clipboardData.m_lengthRTF);
                    extendedClipboardDataMessage.AddFlag(clipRTF);
                    m_bNeedToProvide = true;
                }
                else if (!(overrideFlags & clipRequest))
                {
                    m_bNeedToNotify = true;
                }
            }
            if (clipboardData.m_lengthHTML != 0)
            {
                extendedClipboardDataNotifyMessage.AddFlag(clipHTML);

                if ((clipboardData.m_lengthHTML <= settings.m_nLimitHTML) || (overrideFlags & clipHTML))
                {
                    compressedStream.writeU32(static_cast<rdr::U32>(clipboardData.m_lengthHTML));
                    compressedStream.writeBytes(clipboardData.m_pDataHTML, clipboardData.m_lengthHTML);
                    extendedClipboardDataMessage.AddFlag(clipHTML);
                    m_bNeedToProvide = true;
                }
                else if (!(overrideFlags & clipRequest))
                {
                    m_bNeedToNotify = true;
                }
            }
            if (clipboardData.m_lengthDIB != 0)
            {
                extendedClipboardDataNotifyMessage.AddFlag(clipDIB);

                if ((clipboardData.m_lengthDIB <= settings.m_nLimitDIB) || (overrideFlags & clipDIB))
                {
                    compressedStream.writeU32(static_cast<rdr::U32>(clipboardData.m_lengthDIB));
                    compressedStream.writeBytes(clipboardData.m_pDataDIB, clipboardData.m_lengthDIB);
                    extendedClipboardDataMessage.AddFlag(clipDIB);
                    m_bNeedToProvide = true;
                }
                else if (!(overrideFlags & clipRequest))
                {
                    m_bNeedToNotify = true;
                }
            }

            compressedStream.flush();
        }

        if (m_bNeedToProvide)
        {
            extendedClipboardDataMessage.AppendBytes(const_cast<BYTE*>(reinterpret_cast<const BYTE *>(memStream.data())), memStream.length());
            memStream.clear();
        }

        m_strLastCutText = L"";
        if (!settings.m_bSupportsEx && (clipboardData.m_lengthText > 0) && (clipboardData.m_lengthText < settings.m_nLimitText))
        {
            // Convert UTF-8 to UTF-16 and store directly (no CP_ACP conversion needed)
            int nConvertedSize =
                ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<LPCSTR>(clipboardData.m_pDataText), clipboardData.m_lengthText, nullptr, 0);

            if (nConvertedSize > 0)
            {
                wchar_t* clipStr = new wchar_t[static_cast<unsigned long long>(nConvertedSize + 1)];
                int nFinalConvertedSize = ::MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<LPCSTR>(clipboardData.m_pDataText),
                                                              clipboardData.m_lengthText, reinterpret_cast<LPWSTR>(clipStr), nConvertedSize);

                if (nFinalConvertedSize > 0)
                {
                    clipStr[nFinalConvertedSize] = L'\0';
                    // Store Unicode directly - no CP_ACP conversion, preserves all characters
                    m_strLastCutText.assign(clipStr, static_cast<unsigned long long>(nFinalConvertedSize));
                }

                delete[] clipStr;
            }
        }

        m_crc = clipboardData.m_crc;

        if ((!(settings.m_remoteCaps & clipNotify)) || (overrideFlags != 0))
        {
            m_bNeedToNotify = false;
            extendedClipboardDataNotifyMessage.Reset();
        }
    }

    return m_bNeedToProvide || m_bNeedToNotify;
}
